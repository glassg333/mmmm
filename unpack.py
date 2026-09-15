import base64
import zipfile
import os

with open("monomachine_source_base64.txt", "r") as f:
    raw = f.read()

# remove whitespace/newlines
clean = "".join(raw.split())
data = base64.b64decode(clean)

with open("monomachine_full_source.zip", "wb") as f:
    f.write(data)

with zipfile.ZipFile("monomachine_full_source.zip", "r") as z:
    z.extractall(".")

print(f"Successfully extracted {len(z.namelist())} files from monomachine_full_source.zip!")
