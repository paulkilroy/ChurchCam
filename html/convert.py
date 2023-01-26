# Python program to
# demonstrate readline()


files = [ "config.html", "bootstrap.bundle.min.js", "bootstrap.min.css", "headers.css", "validate-forms.js" ]
outputFile = open('Files.ino', 'w')


for f in files: 
	inputFile = open(f, 'r')
	f = f.replace(".", "_")
	f = f.replace("-", "_")
	outputFile.write(f'const char {f} PROGMEM[] = \n')

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
		if( f == 'config_html' ):
			line = line.replace("%", "%%")

		if line == "];":
			skipJSON = False

		if skipJSON:
			continue

		#Entire file is one line, so remove comments like this
		if line.startswith("//"):
			continue

		if line == "ATEMCameras = [":
			line = "ATEMCameras = [ %ATEMCameras%"
			skipJSON = True

		if line == "DiscoveredCameras = [":
			line = "DiscoveredCameras = [ %DiscoveredCameras%"
			skipJSON = True

		outputFile.write("\"" + line + "\\n\"\n");

	outputFile.write(";\n")
	inputFile.close()

outputFile.close()

