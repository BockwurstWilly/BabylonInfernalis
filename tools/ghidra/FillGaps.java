// Makes the function list cover the code ranges completely, as N64Recomp needs every function (also those only
// reached through function pointers). Repeats until stable: in every gap between functions, skip zero padding
// and create a function at the first non-zero word. Functions outside the code ranges (false finds in data) are
// removed. Code ranges: boot text without rspboot and the RSP microcode, and the vm_paged text.
//@category N64

import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;

public class FillGaps extends GhidraScript {

	static final long[][] CODE = {
			{ 0x40000400L, 0x40001700L }, // boot text up to rspboot_text
			{ 0x400017D0L, 0x400371B0L }, // boot text from after rspboot up to f5_gfx_ucode_text
			{ 0x4003AE50L, 0x40044C00L }, // libultra
			{ 0x400475B0L, 0x400475CCL }, // cache_create_dirty_range, a lone function inside .data (6 callers)
			{ 0x41000000L, 0x410B49B0L }, // vm_paged text (rodata follows: floats, "backdrop/%s_RM%03d")
	};

	@Override
	protected void run() throws Exception {
		FunctionManager fm = currentProgram.getFunctionManager();
		int removed = 0;
		for (Function f : fm.getFunctions(true)) {
			long e = f.getEntryPoint().getOffset();
			boolean inBootOrVm = (e >= 0x40000400L && e < 0x4004D030L) || (e >= 0x41000000L && e < 0x410BE620L);
			if (inBootOrVm && !inCode(e)) {
				println("remove " + f.getName() + " @" + f.getEntryPoint());
				clearListing(f.getBody());
				fm.removeFunction(f.getEntryPoint());
				removed++;
			}
		}
		int created = 0;
		for (int pass = 0; pass < 20; pass++) {
			int n = 0;
			for (long[] r : CODE) {
				long a = r[0];
				while (a < r[1]) {
					Function f = getFunctionContaining(toAddr(a));
					if (f != null) {
						a = Math.max(a + 4, f.getBody().getMaxAddress().getOffset() + 1);
						a = (a + 3) & ~3L;
						continue;
					}
					var data = getDataContaining(toAddr(a));
					if (data != null && data.isDefined()) {
						a = (data.getMaxAddress().getOffset() + 4) & ~3L;
						continue;
					}
					if (getInt(toAddr(a)) == 0) {
						a += 4;
						continue;
					}
					Address s = toAddr(a);
					if (getInstructionAt(s) == null) {
						new DisassembleCommand(s, new AddressSet(s, toAddr(r[1] - 1)), true).applyTo(currentProgram);
					}
					Function nf = getInstructionAt(s) == null ? null : createFunction(s, null);
					if (nf == null) {
						println("could not create function at " + s);
						a += 4;
						continue;
					}
					n++;
					a = nf.getBody().getMaxAddress().getOffset() + 1;
				}
			}
			created += n;
			if (n == 0) {
				break;
			}
		}
		println("removed " + removed + ", created " + created + ", total " + fm.getFunctionCount());
	}

	private boolean inCode(long a) {
		for (long[] r : CODE) {
			if (a >= r[0] && a < r[1]) {
				return true;
			}
		}
		return false;
	}
}
