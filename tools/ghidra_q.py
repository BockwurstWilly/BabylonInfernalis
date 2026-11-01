"""Run a batch of GhidraMCP tool calls against the ghidra-indy server from .mcp.json.

usage: ghidra_q.py '<json list of [tool, {args}]>'   or   ghidra_q.py @calls.json
Prints each result; the server is started once for the whole batch (renames are saved on exit).
"""
import asyncio
import json
import os
import sys

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

CFG = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), ".mcp.json")


async def main(calls) -> None:
    srv = json.load(open(CFG))["mcpServers"]["ghidra-indy"]
    params = StdioServerParameters(command=srv["command"], args=srv["args"], env=dict(os.environ, **srv["env"]))
    async with stdio_client(params) as (r, w):
        async with ClientSession(r, w) as s:
            await s.initialize()
            for tool, args in calls:
                res = await s.call_tool(tool, args)
                text = "".join(getattr(c, "text", "") for c in res.content)
                try:
                    obj = json.loads(text)
                    if isinstance(obj, dict) and "code" in obj:
                        text = obj["code"]
                except ValueError:
                    pass
                print(f"===== {tool} {json.dumps(args)}\n{text}")


arg = sys.argv[1]
calls = json.load(open(arg[1:])) if arg.startswith("@") else json.loads(arg)
asyncio.run(main(calls))
