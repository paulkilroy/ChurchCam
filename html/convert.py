# Python program to
# demonstrate readline()

import re
import os


# iles = [ "html/config.html", "html/restart.html", "html/bootstrap.bundle.min.js", "html/bootstrap.min.css", "html/headers.css", "html/validate-forms.js" ]
files = [ "html/config.html",  "html/bootstrap.bundle.min.js", "html/bootstrap.min.css", "html/headers.css", "html/validate-forms.js" ]
outputFile = open('src/WebFiles.cpp', 'w')

outputFile.write(f'#include <pgmspace.h>\n')

for f in files: 
	inputFile = open(f, 'r')
	fn = os.path.basename(inputFile.name)
	fn = fn.replace(".", "_")
	fn = fn.replace("-", "_")
	outputFile.write(f'extern const char {fn} PROGMEM[] = \n')

	skipJSON = False
	while True:
		# Get next line from file
		line = inputFile.readline()

		# if line is empty
		# end of file is reached
		if not line:
			break
		line = line.strip()
		line = line.replace("\\", "\\\\")
		line = line.replace("\"", "\\\"")
		if( fn == 'config_html' or fn == 'restart.html' ):
			line = line.replace("%", "%%")
			# re.sub(r'(.*)-abc(\.jpg)$', '\g<1>\g<2>', string)
			# Undo the replace we just did it its surrounding a variable name
			# There is probably a better way to do this
			line = re.sub(r'%%(\w+)%%',r'%\1%',line)

			#line = line.replace("<!-- SERVER_VARS -->", "%SERVER_VARS%")


		if line == "];":
			skipJSON = False
			#continue

		if skipJSON:
			continue

		#Entire file is one line, so remove comments like this
		if line.startswith("//"):
			continue

		if line == "Networks = [":
			line = "Networks = [\\n%Networks%"
			skipJSON = True
			#continue
		
		if line == "ATEMSwitcher = [":
			line = "ATEMSwitcher = [\\n%ATEMSwitcher%"
			skipJSON = True
			#continue

		if line == "ATEMCameras = [":
			line = "ATEMCameras = [\\n%ATEMCameras%"
			skipJSON = True
			#continue

		if line == "DiscoveredCameras = [":
			line = "DiscoveredCameras = [ %DiscoveredCameras%"
			skipJSON = True
			#continue

		outputFile.write("\"" + line + "\\n\"\n");

	outputFile.write(";\n")
	inputFile.close()

outputFile.close()

