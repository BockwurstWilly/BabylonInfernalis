// Prepares a raw import of the Indy64 boot segment (ROM 0x1000.., BinaryLoader base 0x40000400).
//
// Memory model of the game (see entrypoint / TLB setup at 0x80000880 and 0x80000F08):
//   TLB entry 0 maps virtual 0x40000000-0x407FFFFF (2 x 4 MiB pages) to physical RDRAM 0-8 MiB,
//   so the game itself runs at 0x4xxxxxxx; kseg0 (0x80..) and kseg1 (0xA0..) are mirrors of the same RAM.
//   0x41000000+ is demand-paged by the TLB refill handler at 0x800009CC (page table pointer at 0x80000464).
//   The pages are raw ROM: 0x41000000 + x == ROM 0x4DC30 + x, 0xBE620 bytes (vm_cart_base, main_thread).
//   The script loads that window from the ROM given as script argument. Names live in symbols/symbols.csv
//   and are applied by ApplySymbols.java.
//
// Import only the real boot segment, ROM 0x1000-0x4DC30 (code+data up to bss start 0x4004D030).
//@category N64

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.SourceType;

public class N64Setup extends GhidraScript {

	private static final long RAM = 0x40000000L;
	private static final long RAM_SIZE = 0x800000L; // 8 MiB with Expansion Pak
	private static final long VM = 0x41000000L;
	private static final long VM_ROM = 0x4DC30L;
	private static final long VM_SIZE = 0xBE620L; // main_thread DMAs exactly this much in non-paging mode

	@Override
	protected void run() throws Exception {
		Memory mem = currentProgram.getMemory();
		MemoryBlock boot = mem.getBlock(addr(RAM + 0x400));
		boot.setName("boot");
		boot.setPermissions(true, true, true);

		long bootEnd = boot.getEnd().getOffset() + 1;
		mem.createUninitializedBlock("lowmem", addr(RAM), 0x400, false).setPermissions(true, true, false);
		mem.createUninitializedBlock("rdram", addr(bootEnd), RAM + RAM_SIZE - bootEnd, false)
				.setPermissions(true, true, true);
		// Mirrors are not executable so analysis stays on the 0x4xxxxxxx view of the code.
		mem.createByteMappedBlock("kseg0", addr(0x80000000L), addr(RAM), RAM_SIZE, false)
				.setPermissions(true, true, false);
		mem.createByteMappedBlock("kseg1", addr(0xA0000000L), addr(RAM), RAM_SIZE, false)
				.setPermissions(true, true, false);

		String[] args = getScriptArgs();
		try (java.io.RandomAccessFile rom = new java.io.RandomAccessFile(args[0], "r")) {
			byte[] vm = new byte[(int) VM_SIZE];
			rom.seek(VM_ROM);
			rom.readFully(vm);
			mem.createInitializedBlock("vm_paged", addr(VM), new java.io.ByteArrayInputStream(vm), VM_SIZE,
					monitor, false).setPermissions(true, true, true);
		}
		setPlateComment(addr(VM), "Demand-paged window: 0x41000000 + x is ROM 0x4DC30 + x");

		io("SP_DMEM", 0xA4000000L, 0x1000);
		io("SP_IMEM", 0xA4001000L, 0x1000);
		io("SP_REG", 0xA4040000L, 0x20);
		io("SP_PC", 0xA4080000L, 0x8);
		io("DPC_REG", 0xA4100000L, 0x20);
		io("MI_REG", 0xA4300000L, 0x10);
		io("VI_REG", 0xA4400000L, 0x38);
		io("AI_REG", 0xA4500000L, 0x18);
		io("PI_REG", 0xA4600000L, 0x34);
		io("RI_REG", 0xA4700000L, 0x20);
		io("SI_REG", 0xA4800000L, 0x1C);
		io("PIF_RAM", 0xBFC007C0L, 0x40);

		label(0xA4040010L, "SP_STATUS_REG");
		label(0xA4040018L, "SP_SEMAPHORE_REG");
		label(0xA4100000L, "DPC_START_REG");
		label(0xA4100004L, "DPC_END_REG");
		label(0xA410000CL, "DPC_STATUS_REG");
		label(0xA4300008L, "MI_INTR_REG");
		label(0xA430000CL, "MI_INTR_MASK_REG");
		label(0xA4400000L, "VI_STATUS_REG");
		label(0xA4400004L, "VI_ORIGIN_REG");
		label(0xA4400010L, "VI_CURRENT_REG");
		label(0xA4500000L, "AI_DRAM_ADDR_REG");
		label(0xA4500004L, "AI_LEN_REG");
		label(0xA450000CL, "AI_STATUS_REG");
		label(0xA4600000L, "PI_DRAM_ADDR_REG");
		label(0xA4600004L, "PI_CART_ADDR_REG");
		label(0xA4600008L, "PI_RD_LEN_REG");
		label(0xA460000CL, "PI_WR_LEN_REG");
		label(0xA4600010L, "PI_STATUS_REG");
		label(0xA4800000L, "SI_DRAM_ADDR_REG");
		label(0xA4800010L, "SI_PIF_ADDR_RD64B_REG");
		label(0xA4800018L, "SI_STATUS_REG");

		label(0x40000454L, "vm_fault_state");
		label(0x40000464L, "vm_page_table_ptr");

		// RSP microcode found by COP2 density (tools/find_rsp.py), ROM 0x37F00-0x3BA00 -> RAM 0x40037300.
		label(0x40037300L, "rsp_ucode_region");
		setEOLComment(addr(0x40037300L), "RSP microcode text, ROM 0x37F00-0x3BA00 (not VR4300 code)");

		// Boot code runs from kseg0 before the TLB is set up; analyze it at its RAM alias.
		fn(0x40000400L, "entrypoint");
		currentProgram.getSymbolTable().addExternalEntryPoint(addr(0x40000400L));
		fn(0x40000880L, "tlb_clear_all");
		fn(0x40000F08L, "tlb_map_entry");
		fn(0x400009CCL, "tlb_refill_handler");
		fn(0x40000AE0L, "general_exception_handler");
		fn(0x40000DDCL, "vm_page_lru_touch");
		fn(0x40036F64L, "boot_main");
	}

	private Address addr(long a) {
		return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(a);
	}

	private void fn(long a, String name) throws Exception {
		disassemble(addr(a));
		createFunction(addr(a), name);
		createLabel(addr(a), name, true, SourceType.USER_DEFINED);
	}

	private void io(String name, long start, long size) throws Exception {
		MemoryBlock b = currentProgram.getMemory().createUninitializedBlock(name, addr(start), size, false);
		b.setPermissions(true, true, false);
		b.setVolatile(true);
	}

	private void label(long a, String name) throws Exception {
		createLabel(addr(a), name, true, SourceType.USER_DEFINED);
	}
}
