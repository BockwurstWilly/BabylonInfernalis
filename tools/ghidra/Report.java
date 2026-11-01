// Prints function count, code coverage and instructions that reference missing memory.
//@category N64

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;

public class Report extends GhidraScript {
	@Override
	protected void run() throws Exception {
		Listing l = currentProgram.getListing();
		println("functions: " + currentProgram.getFunctionManager().getFunctionCount());
		long code = 0;
		InstructionIterator it = l.getInstructions(true);
		int shown = 0;
		while (it.hasNext()) {
			Instruction ins = it.next();
			code += ins.getLength();
			for (Reference r : ins.getReferencesFrom()) {
				Address to = r.getToAddress();
				if (to.isMemoryAddress() && currentProgram.getMemory().getBlock(to) == null && shown < 15
						) {
					println(ins.getAddress() + "  " + ins + "  -> " + to + "  (" + r.getReferenceType() + ")");
					shown++;
				}
			}
		}
		println("instruction bytes: " + code);
		String[] addrs = { "40036f64" };
		for (String a : addrs) {
			Address s = toAddr(a);
			println("--- " + a);
			Instruction ins = l.getInstructionAt(s);
			for (int i = 0; i < 24 && ins != null; i++, ins = ins.getNext()) {
				println(ins.getAddress() + "  " + ins);
			}
		}
	}
}
