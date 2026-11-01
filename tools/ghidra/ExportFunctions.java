// Exports all functions of the boot and vm_paged blocks as CSV: entry,name,body_min,body_max,ranges,instructions.
// Script argument: output path.
//@category N64

import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;

public class ExportFunctions extends GhidraScript {
	@Override
	protected void run() throws Exception {
		try (PrintWriter out = new PrintWriter(getScriptArgs()[0])) {
			out.println("entry,name,block,body_min,body_max,ranges,instructions");
			for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
				MemoryBlock b = getMemoryBlock(f.getEntryPoint());
				if (b == null || !(b.getName().equals("boot") || b.getName().equals("vm_paged"))) {
					continue;
				}
				AddressSetView body = f.getBody();
				int n = 0;
				var it = currentProgram.getListing().getInstructions(body, true);
				while (it.hasNext()) {
					it.next();
					n++;
				}
				out.printf("0x%08X,%s,%s,0x%08X,0x%08X,%d,%d%n", f.getEntryPoint().getOffset(), f.getName(), b.getName(),
						body.getMinAddress().getOffset(), body.getMaxAddress().getOffset(), body.getNumAddressRanges(), n);
			}
		}
	}
}
