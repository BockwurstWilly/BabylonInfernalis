// Applies symbols/symbols.csv (address,name,kind,size,note) to the current program; the note becomes a plate comment.
//   func with size: the function body is set to exactly [address, address+size); functions starting inside that
//                   range are removed first (Ghidra split hand-written routines at an early jr ra).
//   func without size: the function is created if missing and renamed.
//   data with size: the range is cleared and defined as a byte array, so neither analysis nor FillGaps treats it
//                   as code (tables and constants inside hand-written assembly).
// Script argument: path to symbols.csv.
//@category N64

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.List;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.data.ArrayDataType;
import ghidra.program.model.data.ByteDataType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;

public class ApplySymbols extends GhidraScript {
	@Override
	protected void run() throws Exception {
		int n = 0;
		try (BufferedReader r = new BufferedReader(new FileReader(getScriptArgs()[0]))) {
			r.readLine(); // header
			for (String line; (line = r.readLine()) != null;) {
				if (line.isBlank()) {
					continue;
				}
				List<String> f = csv(line);
				Address a = toAddr(Long.decode(f.get(0)));
				String name = f.get(1);
				long size = f.get(3).isEmpty() ? 0 : Long.decode(f.get(3));
				String note = f.size() > 4 ? f.get(4) : "";
				try {
					if (f.get(2).equals("func")) {
						applyFunction(a, name, size);
					} else {
						if (size > 0 && getMemoryBlock(a) != null && getMemoryBlock(a).isInitialized()) {
							Address end = a.add(size - 1);
							for (Function inner : currentProgram.getFunctionManager().getFunctions(new AddressSet(a, end), true)) {
								removeFunction(inner);
							}
							clearListing(a, end);
							createData(a, new ArrayDataType(ByteDataType.dataType, (int) size, 1));
						}
						createLabel(a, name, true, SourceType.USER_DEFINED);
					}
					if (!note.isEmpty()) {
						setPlateComment(a, note);
					}
				} catch (Exception e) {
					println("failed " + name + " @" + a + ": " + e.getMessage());
				}
				n++;
			}
		}
		println("applied symbols: " + n);
	}

	private void applyFunction(Address a, String name, long size) throws Exception {
		if (size > 0) {
			Address end = a.add(size - 1);
			for (Function inner : currentProgram.getFunctionManager().getFunctions(new AddressSet(a, end), true)) {
				if (!inner.getEntryPoint().equals(a)) {
					removeFunction(inner);
				}
			}
			if (getInstructionAt(a) == null) {
				disassemble(a);
			}
		} else if (getFunctionAt(a) == null) {
			disassemble(a);
		}
		Function fn = getFunctionAt(a);
		if (fn == null) {
			fn = createFunction(a, name);
		}
		if (fn == null) {
			println("could not create " + name + " @" + a);
			return;
		}
		fn.setName(name, SourceType.USER_DEFINED);
		if (size > 0) {
			fn.setBody(new AddressSet(a, a.add(size - 1)));
		}
	}

	private static List<String> csv(String line) {
		List<String> out = new ArrayList<>();
		StringBuilder cur = new StringBuilder();
		boolean q = false;
		for (char c : line.toCharArray()) {
			if (c == '"') {
				q = !q;
			} else if (c == ',' && !q) {
				out.add(cur.toString());
				cur.setLength(0);
			} else {
				cur.append(c);
			}
		}
		out.add(cur.toString());
		return out;
	}
}
