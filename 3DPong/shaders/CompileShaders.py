#compiles the shaders into SPRV format

import sys
import os
import subprocess

#the tool for compiling shaders
SPRV_COMPILING_TOOL_DIR = "C:/VulkanSDK/1.3.283.0/Bin/glslc.exe"

#gets the path passed in for the code dir and the compiled code dir, or sets to default
codeDir = "Code"
outputDir = "Compiled"
if(len(sys.argv) > 1):
    codeDir = sys.argv[1]
    outputDir = sys.argv[2]

#creates the output directory if it doesn't exist
if(os.path.isdir(outputDir) == False):
    os.mkdir(outputDir)

#gets the files in the code directory
shaders = os.listdir(codeDir)

#goes through the code dir and compile them
for shader in shaders:
    subprocess.call([SPRV_COMPILING_TOOL_DIR, codeDir + "/" + shader, "-o", outputDir + "/" + shader + ".spv"])
    print(shader + " compiled...")