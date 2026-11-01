"""Smoke test of the ghidra-indy MCP server from .mcp.json: list tools, decompile boot_main."""
import asyncio
import json
import os
import sys

from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client

CFG = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), ".mcp.json")


async def main(address: str) -> None:
    srv = json.load(open(CFG))["mcpServers"]["ghidra-indy"]
    env = dict(os.environ, **srv["env"])
    params = StdioServerParameters(command=srv["command"], args=srv["args"], env=env)
    async with stdio_client(params) as (r, w):
        async with ClientSession(r, w) as s:
            await s.initialize()
            tools = await s.list_tools()
            print("tools:", ", ".join(t.name for t in tools.tools))
            props = next(t for t in tools.tools if t.name == "decompile_function_by_address").inputSchema["properties"]
            res = await s.call_tool("decompile_function_by_address", {next(iter(props)): address})
            print("".join(getattr(c, "text", "") for c in res.content)[:2500])


asyncio.run(main(sys.argv[1] if len(sys.argv) > 1 else "0x40036f64"))
