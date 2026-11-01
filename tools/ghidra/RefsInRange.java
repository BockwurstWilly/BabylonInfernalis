// Lists every reference whose target lies in [start, end) — args: start end (hex).
//@category N64

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class RefsInRange extends GhidraScript {
	@Override
	protected void run() throws Exception {
		Address start = toAddr(Long.decode(getScriptArgs()[0]));
		Address end = toAddr(Long.decode(getScriptArgs()[1]));
		var rm = currentProgram.getReferenceManager();
		var dests = rm.getReferenceDestinationIterator(start, true);
		while (dests.hasNext()) {
			Address to = dests.next();
			if (to.compareTo(end) >= 0) {
				break;
			}
			ReferenceIterator it = rm.getReferencesTo(to);
			while (it.hasNext()) {
				Reference r = it.next();
				Function f = getFunctionContaining(r.getFromAddress());
				println(to + " <- " + r.getFromAddress() + " " + r.getReferenceType() + " in "
						+ (f == null ? "?" : f.getName() + "@" + f.getEntryPoint()));
			}
		}
	}
}
