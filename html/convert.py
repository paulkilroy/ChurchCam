import re
import os
import gzip

files = [ "html/config.html", "html/restart.html", "html/bootstrap.bundle.min.js", "html/bootstrap.min.css", "html/headers.css", "html/validate-forms.js" ]
outputFile = open('src/WebFiles.cpp', 'w')

outputFile.write(f'#include <pgmspace.h>\n')
outputFile.write(f'#include <Arduino.h>\n')

for f in files: 
	inputFile = open(f, 'r')
	fn = os.path.basename(inputFile.name)
	fn = fn.replace(".", "_")
	fn = fn.replace("-", "_")
	outputFile.write(f'extern const char {fn} PROGMEM[] = \n')

	# compress js and css files into byte arrays to send directly to the browser
	if( fn.endswith('js') or fn.endswith('css')):
		print(F"Binary Converting: {fn}")
		inputFile = open(f, 'rb')
		f_out = gzip.open(f+'.gz', 'wb')
		f_out.writelines(inputFile)
		f_out.close()
		inputFile.close()
		gzipFile = open(f+'.gz', 'rb')
		outputFile.write("{ ")
		bytes = 0
		while(gzipFile.peek()):
			b = gzipFile.read(1)
			if( bytes > 0 ):
				outputFile.write(", ")
			outputFile.write("0x" + b.hex())
			bytes = bytes + 1
		outputFile.write(" };\n")
		outputFile.write(f'extern const size_t {fn}_bytes = {bytes};\n\n')
		gzipFile.close()
	elif( fn.endswith('html') ):
		print(F"Processor Converting: {fn}")
		skipJSON = False
		while True:
			# Get next line from file
			line = inputFile.readline()

			# if line is empty
			# end of file is reached
			if not line:
				break

			# make the file smaller by removing whitespace
			line = line.strip()

			# need to be able to escape quotes and other characters inside C strings
			line = line.replace("\\", "\\\\")
			line = line.replace("\"", "\\\"")

			line = line.replace("%", "%%")
			# re.sub(r'(.*)-abc(\.jpg)$', '\g<1>\g<2>', string)
			# Undo the replace we just did it its surrounding a variable name
			# There is probably a better way to do this
			line = re.sub(r'%%(\w+)%%',r'%\1%',line)

			if line == "];":
				skipJSON = False

			if skipJSON:
				continue

			#Entire file is one line, so remove comments like this
			if line.startswith("//"):
				continue

			if line == "Networks = [":
				line = "Networks = [\\n%Networks%"
				skipJSON = True
		
			if line == "ATEMSwitcher = [":
				line = "ATEMSwitcher = [\\n%ATEMSwitcher%"
				skipJSON = True

			if line == "ATEMCameras = [":
				line = "ATEMCameras = [\\n%ATEMCameras%"
				skipJSON = True

			if line == "DiscoveredCameras = [":
				line = "DiscoveredCameras = [ %DiscoveredCameras%"
				skipJSON = True

			outputFile.write("\"" + line + "\\n\"\n")

		outputFile.write(";\n")
		inputFile.close()
	else:
		print(F"ERROR Unknown file: {fn}")
		
outputFile.close()

