import sys

# python prep_exports.py System.map > exports

# this helper script extracts all exportet symbols from System.map
# and creates an export file to be used for objcpy
# this is neccessary when loading modules since vanilla lkl
# does not export all neccassary symbols
dfs_funcs = set()
exports = set()
with open(sys.argv[1], "r") as fd:
   for l in fd.readlines():
      addr, typ, name = l.split()
      if "__ksymtab" in name:
         exports.add(name.replace("__ksymtab_", ""))
      if "dfs" in name:
         dfs_funcs.add(name.replace("dfs$", ""))

for e in exports:
   if e in dfs_funcs:
      print("dfs$%s" % e)
   else:
      print(e)

