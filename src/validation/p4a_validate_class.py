#! /usr/bin/python

# -*- coding: utf-8 -*-

"""
Validation utility for Par4All

Add object oriented organization above PIPS validation.

Introduce the concept of validation class.
"""
import os, commands, string, re, optparse,glob, shutil, stat

class ValidationClass:

###### Init of the validation ######
  def __init__(self):

		self.p4a_root = ''

		for root, subfolders, files in os.walk(os.environ.get("PWD")):
			for file in files:
				if (file == 'p4a_validate_class.py'):
					 self.p4a_root = os.path.dirname(os.path.join(root,file))

		if (not self.p4a_root):
			print ('You need to define P4A_ROOT environment variable')
			exit()

		self.par4ll_validation_dir = self.p4a_root+'/../../packages/PIPS/validation/'
		self.extension = ['.c','.F','.f','.f90','.f95']

		# warning and failed status
		self.warning = ['skipped']
		self.failed = ['changed','failed']

		# get default architecture and tpips/pips
		self.arch=commands.getoutput(self.p4a_root+"/run/makes/arch.sh")

#### Command to test ###
  def command_test(self,directory_test_path,test_name_path,err_file_path,test_file_path):
		if (os.path.isfile(test_name_path+".test")):
			(int_status, output) = commands.getstatusoutput(test_name_path+".test 2> "+err_file_path)

		elif (os.path.isfile(test_name_path+".tpips")):
			(int_status, output) = commands.getstatusoutput("tpips "+test_name_path+".tpips 2> "+err_file_path)

		elif (os.path.isfile(test_name_path+".tpips2")):
			(int_status, output) = commands.getstatusoutput("tpips "+test_name_path+".tpips2 2>&1")

		elif (os.path.isfile(test_name_path+".py")):
			(int_status, output) = commands.getstatusoutput("python "+os.path.basename(test_name_path)+".py 2> "+err_file_path)

		elif (os.path.isfile(directory_test_path+"/default_tpips")):
			# Change flag for default_tpips
			os.chmod(directory_test_path+"/default_tpips", stat.S_IEXEC | stat.S_IREAD | stat.S_IWRITE)
			(int_status, output) = commands.getstatusoutput("FILE="+test_file_path+" WSPACE="+os.path.basename(test_name_path)+" tpips "+directory_test_path+"/default_tpips 2>"+err_file_path)

		elif (os.path.isfile(directory_test_path+"/default_test")):
			# Change flag for default_test
			os.chmod(directory_test_path+"/default_test", stat.S_IEXEC | stat.S_IREAD | stat.S_IWRITE)
			# test_name=file
			# upper=FILE
			upper = os.path.basename(test_name_path).upper()
			(int_status, output) = commands.getstatusoutput("FILE="+test_file_path+" WSPACE="+os.path.basename(test_name_path)+" NAME="+upper+" "+directory_test_path+"/default_test 2>"+err_file_path)

		elif (os.path.isfile(directory_test_path+"/default_pyps.py")):
			# Change flag for default_pyps.py
			os.chmod(directory_test_path+"/default_pyps.py", stat.S_IEXEC | stat.S_IREAD | stat.S_IWRITE)
			(int_status, output) = commands.getstatusoutput("FILE="+test_file_path+" WSPACE="+os.path.basename(test_name_path)+" python "+directory_test_path+"/default_pyps.py 2>"+err_file_path)

		else:
			# Create a err file
			err_file_h = open(err_file_path,'w')
			
			commands.getstatusoutput("Delete "+os.path.basename(test_name_path)+" 2> /dev/null 1>&2")

			(int_status, output) = commands.getstatusoutput("Init -f "+test_file_path+" -d "+os.path.basename(test_name_path)+" 2> "+err_file_path)

			(int_status, output_display) = commands.getstatusoutput("while read module ; do Display -m  $module -w "+os.path.basename(test_name_path)+" ; done < "+os.path.basename(test_name_path)+".database/modules")
			err_file_h.write ('%s' % (output_display))

			commands.getstatusoutput("Delete "+os.path.basename(test_name_path)+" 2> /dev/null 1>&2")

			err_file_h.close()

		return (int_status,output)

#### Function which run tests and save result on result_log ######
  def test_par4all(self,directory_test_path,test_file_path,log_file):
		# .result directory of the test to compare results
		test_file_path = test_file_path.strip('\n')
		(test_name_path, ext) = os.path.splitext(test_file_path)
		test_result_path = test_name_path + '.result'
	
		#output ref of the test
		test_ref_path = test_result_path + '/test'

		# log/summary of the results
		self.file_result = open(log_file,'a')
		status = 'succeeded'

		#check that .result and reference of the test are present. If not, status is "skipped" 
		if (os.path.isdir(test_result_path) != True or (os.path.isfile(test_ref_path) != True and os.path.isfile(test_ref_path+'.'+self.arch) != True)):
			status ='skipped'
		elif (os.path.isfile(test_name_path+".bug") or os.path.isfile(test_name_path+".later")):
			status ='bug-later'
		else:
			# output of the test and error of the tests
			output_file_path = test_result_path+'/'+os.path.basename(test_name_path)+'.out'
			err_file_path = test_name_path + '.err'

			# go to the directory of the test
	 		os.chdir(directory_test_path)

			# remove old .database
			for filename in glob.glob(directory_test_path+'/*.database') :
				shutil.rmtree(filename,ignore_errors=True) 

			# test
			(int_status,output) = self.command_test(directory_test_path,test_name_path,err_file_path,test_file_path)

			# filter out absolute path anyway, there may be some because of
			# cpp or other stuff run by pips, even if relative path names are used.
			output = output.replace(directory_test_path,'.')

			if (os.path.isfile(err_file_path) == True):
				# copy error file on RESULT directories of par4all validation
				shutil.move(err_file_path,self.p4a_root+'/RESULT')
				os.rename(self.p4a_root+"/RESULT/"+os.path.basename(err_file_path),self.p4a_root+"/RESULT/"+os.path.basename(directory_test_path)+"_"+os.path.basename(err_file_path))
		
			if(int_status != 0):
				status = "failed"
			else:
				if(os.path.isfile(test_ref_path+'.'+self.arch) == True):
					ref_path = test_ref_path+'.'+self.arch
				else:
					ref_path = test_ref_path

				reference_filtered_path = ref_path

				out_filtered = output
				reference_filtered = open(ref_path).read()

				# let's apply some filter on output if define by user
				# First try to apply a test_and_out filter on both expected and obtained output
				# Else try to apply test and out filter on test and out output
				if (os.path.isfile(test_result_path + "/test_and_out.filter")):
					# apply the user define filter on both the reference
					os.chmod(test_result_path + "/test_and_out.filter", stat.S_IEXEC | stat.S_IREAD | stat.S_IWRITE)

					output_file_filter_path = test_result_path+'/'+os.path.basename(test_name_path)+'.out.filtered'
					output_file_filter_h = open(output_file_filter_path,'w')
					output_file_filter_h.write ('%s' % (output))
					output_file_filter_h.close()

					(int_status, reference_filtered) = commands.getstatusoutput(test_result_path + "/test_and_out.filter "+reference_filtered_path)
					(int_status, out_filtered) = commands.getstatusoutput(test_result_path + "/test_and_out.filter "+output_file_filter_path)

					# Write new "test reference" filtered
					reference_filter_path = ref_path + '.filtered'
					reference_filter_h = open(reference_filter_path,'w')
					reference_filter_h.write ('%s' % (reference_filtered))
					reference_filter_h.close()

					out_is_filtered=1
					ref_is_filtered=1

				else:
					# apply the user define filter on the reference
					if (os.path.isfile(test_result_path + "/test.filter")):
						os.chmod(test_result_path + "/test.filter", stat.S_IEXEC | stat.S_IREAD | stat.S_IWRITE)
						(int_status, reference_filtered) = commands.getstatusoutput(test_result_path + "/test_and_out.filter "+reference_filtered_path)

						# Write new "test reference" filtered
						reference_filter_path = ref_path + '.filtered'
						reference_filter_h = open(reference_filter_path,'w')
						reference_filter_h.write ('%s' % (reference_filtered))
						reference_filter_h.close()
						ref_is_filtered=1

					# apply the user define filter on the output
					if (os.path.isfile(test_result_path + "/out.filter")):
						os.chmod(test_result_path + "/out.filter", stat.S_IEXEC | stat.S_IREAD | stat.S_IWRITE)
						(int_status, out_filtered) = commands.getstatusoutput(test_result_path + "/test_and_out.filter "+output_file_filter_path)
						out_is_filtered=1

				output_file_h = open(output_file_path,'w')
				output_file_h.write ('%s' % (out_filtered))	
				output_file_h.close()

				# Diff between output filtered and reference filtered
				if (reference_filtered.replace(" ","").replace('\n','') != out_filtered.replace(" ","").replace('\n','')):
					status = 'changed'
				else:
					#status of the test
					status = 'succeeded'

		# Write status
		self.file_result.write ('%s: %s\n' % (status,test_file_path.replace(self.par4ll_validation_dir,'')))
		self.file_result.close()
	
		# Return to validation Par4All
		os.chdir(self.p4a_root)
		return status

###### Validate only test what we want ######
  def valid_par4all(self):
		os.chdir(self.p4a_root)
		if os.path.isfile('par4all_validation.txt'):
			f = open("par4all_validation.txt")
		else:
			print ('No par4all_validation.txt file in %s. Create one before launch validation par4all'%(self.p4a_root))
			exit()

		# Create directory for result
		if (os.path.isdir("RESULT") == True):
			shutil.rmtree("RESULT", ignore_errors=True)
		os.mkdir("RESULT")

		if os.path.isfile('p4a_log.txt'):
			os.remove("p4a_log.txt")

		# Open the file where par4all tests are:
		for line in f:
			# In case of the test is written like Directory_test\test.f instead os Directory_test/test.f
			line = line.replace('\\','/')
			# delete .f, .c and .tpips of the file name
			(root, ext) = os.path.splitext(line)

			# split to have: link to folder of the test and name of the test
			directory=root.split("/")
			directory_test = self.par4ll_validation_dir

			for j in range(0,len(directory)-1):
				directory_test = directory_test+'/'+directory[j]

			print (('# Considering %s')%(line.strip('\n')))

			ext = ext.strip('\n').strip(' ')
			
			if(ext in self.extension):
				if (os.path.isdir(directory_test) != True ):
					print('%s is not a directory into packages/PIPS/validation'%(directory_test.replace(self.par4ll_validation_dir+'/','')))
				elif (os.path.isfile(self.par4ll_validation_dir+line.strip('\n').strip(' ')) != True):
					print('%s is not a file into packages/PIPS/validation/'%(line.strip('\n')))
				else:
					# Run test
					status = self.test_par4all(directory_test,self.par4ll_validation_dir+line,'p4a_log.txt')

			else:
				print ("To test %s, use an extension like %s\n"%(line.strip('\n'),self.extension))

		f.close()
		(nb_warning,nb_failed,nb_test) = self.count_failed_warn("p4a_log.txt")
		print('%s failed and %s warning (skipped) in %s tests'%(nb_failed,nb_warning,nb_test))

### .result test to test ####
  def result_test(self,directory_test,resultdir_list,log_file):
		for result_dir in resultdir_list:

			# Search a correspondig file of .result folder
			test_name_list = self.search_file(result_dir.replace('.result',''))

			for test_name_path in test_name_list:
				# Test file
				status = self.test_par4all(directory_test,test_name_path,log_file)
  
#### Directory to test - Recursive to enter in subdirectories ####
  def recursive_dir_test(self,dir_list,log_file):
		# Check that list is not empty, so there is directory to test
		if (len(dir_list) != 0):
			i = 0
			# List of subdirectories to test
			dir_sublist = list()

			# Browse directory
			for i in range(0,len(dir_list)):
				directory_test = dir_list[i]
				# List all .result directories
				resultdir_list = glob.glob(directory_test+'/*.result')
				print (('# Considering %s')%(directory_test.replace(self.par4ll_validation_dir,'').strip('\n')))

				# Test
				self.result_test(directory_test,resultdir_list,log_file)

				# Check subdirectories to test and skipped status (no correspondig .result folder)
				listing = os.listdir(directory_test)
				for dirfile in listing:
					self.subdir_list_test(directory_test,log_file,resultdir_list,dirfile,dir_sublist)

			self.recursive_dir_test(dir_sublist,log_file)
		(nb_warning,nb_failed,nb_test) = self.count_failed_warn(log_file)

### Return number of failed, warning and test ####
  def count_failed_warn(self,log_file):
		nb_warning = 0
		nb_failed  = 0
		nb_test = 0

		if os.path.exists(log_file):
			log_file_h = open(log_file,'r')
			readline_log_file = log_file_h.readlines()

			# Number of test
			nb_test = len(readline_log_file)

			log_file_str = str(readline_log_file)

			# Number of warning
			i = 0
			for i in range(0,len(self.warning)):
				nb_warning = nb_warning + int(log_file_str.count(self.warning[i]+':'))

			# Number of failed
			i = 0
			for i in range(0,len(self.failed)):
				nb_failed = nb_failed + int(log_file_str.count(self.failed[i]+':'))

			log_file_h.close()
		return (nb_warning,nb_failed,nb_test)

### Check subdirectories to test and skipped status (no correspondig .result folder) ####
  def subdir_list_test(self,directory_test,log_file,resultdir_list,dirfile,dir_sublist):
		(root, ext) = os.path.splitext(dirfile)
		if os.path.isdir(directory_test+'/'+dirfile):
			if (ext != '.result'):
				if (ext == '.sub'):
					dir_sublist.append(directory_test+'/'+dirfile)
				else:
					# Is it in the makefile?
					for line in open(directory_test+"/Makefile"):
						if ((dirfile in line) and ("D.sub" in line)) :
							dir_sublist.append(directory_test+'/'+dirfile)
		elif ((os.path.isfile(directory_test+'/'+dirfile)) and ((directory_test+'/'+root+'.result' in resultdir_list) != True) and (ext in self.extension)):
			self.file_result = open(log_file,'a')
			self.file_result.write("skipped: "+(directory_test+'/'+dirfile).replace(self.par4ll_validation_dir,'')+'\n')
			self.file_result.close()

		return dir_sublist

### List tests in directories and subdirectories and check that it's not present in par4all_validation.txt ####
  def recursive_list_test(self,dir_list,par4all_string):
		# Check that list is not empty, so there is directory to test
		if (len(dir_list) != 0):
			i = 0
			# List of subdirectories to test
			dir_sublist = list()

			# Browse directory
			for i in range(0,len(dir_list)):
				directory_test = dir_list[i]
				resultdir_list = glob.glob(directory_test+'/*.result')

				for result_dir in resultdir_list:
					# Search a correspondig file of .result folder
					test_name_list = self.search_file(result_dir.replace('.result',''))
					for test_name_path in test_name_list:
						# Test is find. Check that it is present into par4all_validation.txt
						test_string = test_name_path.replace(self.par4ll_validation_dir,'').strip('\n')

						# Is test is in par4all_validation.txt ?
						if int(par4all_string.count(test_string)) == 0:
							diff_test_h = open('diff.txt','a')
							diff_test_h.write(test_name_path.replace(self.par4ll_validation_dir,'')+'\n')
							diff_test_h.close()

				# Check subdirectories to test and skipped status (no correspondig .result folder)
				listing = os.listdir(directory_test)
				for dirfile in listing:
					dir_sublist = self.subdir_list_test(directory_test,"diff.txt",resultdir_list,dirfile,dir_sublist)

			self.recursive_list_test(dir_sublist,par4all_string)

### Find file (c, fortran, etc...) of the corresponding .result test ###
  def search_file(self,filename):
		file_found = 0
		files = list()

		# List all tests file with corresponding .result folder
		for ext in self.extension:
			if os.path.exists(filename+ext):
				files.append(filename+ext)
				file_found = 1

		if file_found == 0:
			files.append(filename+'.result')

		return files

###### Validate all tests (done by "default" file) ######
  def valid_pips(self):
		os.chdir(self.p4a_root)
		# Create directory for result
		if (os.path.isdir("RESULT") == True):
			shutil.rmtree("RESULT", ignore_errors=True)
		os.mkdir("RESULT")

		default_file_path = self.par4ll_validation_dir+"defaults"

		default_file = open(default_file_path)

		if os.path.isfile('pips_log.txt'):
			os.remove("pips_log.txt")

		for line in default_file:
			if (not re.match('#',line)):
				# List all directories that we must test
				line  = line.strip('\n')
				dir_list = [self.par4ll_validation_dir+line];
				self.recursive_dir_test(dir_list,'pips_log.txt')

		(nb_warning,nb_failed,nb_test) = self.count_failed_warn('pips_log.txt')
		print('%s failed and %s warning (skipped) in %s tests.'%(nb_failed,nb_warning,nb_test))
		default_file.close()

###### Diff between p4a and pips options ######
  def diff(self):

		os.chdir(self.p4a_root)
		# Read default file to build a file with all tests
		default_file = open(self.par4ll_validation_dir+"defaults")

		diff_file = open('diff.txt','w')
		diff_file.close()

		if os.path.isfile(self.p4a_root+'/par4all_validation.txt'):
			par4all_h = open(self.p4a_root+"/par4all_validation.txt")
			par4all_string = par4all_h.read()
			par4all_string = par4all_string.replace('\\','/').strip('\n')
			par4all_h.close()
		else:
			par4all_string = ''

		# Parse all tests done by default file in pips validation and build a file with all tests which are not written in par4all_validation.txt
		for line in default_file:
			if (not re.match('#',line)):
				line  = line.strip('\n')
				dir_list = [self.par4ll_validation_dir+line]
				self.recursive_list_test(dir_list,par4all_string)

		(nb_warning,nb_failed,nb_test) = self.count_failed_warn('diff.txt')
		print('%i tests are not done by --p4a options or their status is "skipped"'%(nb_test))

###### Validate all tests of a specific directory ################
  def valid_dir(self,arg_dir):
		os.chdir(self.p4a_root)
		if os.path.isfile('directory_log.txt'):
			os.remove("directory_log.txt")

		# Create directory for result
		if (os.path.isdir("RESULT") == True):
			shutil.rmtree("RESULT", ignore_errors=True)
		os.mkdir("RESULT")

		#read the directory
		i = 0

		for i in range(0,len(arg_dir)):
			directory_name = arg_dir[i]
			directory_test = [self.par4ll_validation_dir+directory_name]

			# Is it a valid directory?
			if (os.path.isdir(self.par4ll_validation_dir+directory_name) != True):
				print ("%s does not exist or it's not a repository"%(directory_name))
			else:
				# Test
				self.recursive_dir_test(directory_test,'directory_log.txt')
		
		(nb_warning,nb_failed,nb_test) = self.count_failed_warn('directory_log.txt')
		print('%s failed and %s warning (skipped) in %s tests'%(nb_failed,nb_warning,nb_test))

###### Validate all desired tests ################
  def valid_test(self,arg_test):

		os.chdir(self.p4a_root)
		# Create directory for result
		if (os.path.isdir("RESULT") == True):
			shutil.rmtree("RESULT", ignore_errors=True)
		os.mkdir("RESULT")

		#read the tests
		i = 0

		for i in range(0,len(arg_test)):
			test_array=arg_test[i].split("/")
			j = 0

			# Directory of the test
			directory_test = self.par4ll_validation_dir
			for j in range(0,len(test_array)-1):
				directory_test = directory_test+'/'+test_array[j]

			# File to test
			file_tested = directory_test+'/'+test_array[len(test_array)-1]

			# Check that directory and test exist
			if (os.path.isdir(directory_test) != True):
				print('%s is not a directory into packages/PIPS/validation'%(directory_test.replace(self.par4ll_validation_dir+'/','')))
			elif (os.path.isfile(file_tested) != True):
				print('%s is not a file into packages/PIPS/validation/%s'%(arg_test[i],directory_test.replace(self.par4ll_validation_dir,'')))
			else:
				# Check that extension of the file is OK
				(root, ext) = os.path.splitext(test_array[len(test_array)-1])
				if(ext in self.extension):
					# Test
					status = self.test_par4all(directory_test, file_tested,'test_log.txt')
					print('%s : %s'%(arg_test[i],status))
				else:
					print('%s : Not done (extension must be %s)'%(arg_test[i],self.extension))

###################### Main -- Options #################################
def main():
	usage = "usage: python %prog [options]"
	parser = optparse.OptionParser(usage=usage)
	parser.add_option("--pips", action="store_true", dest="pips", help = "Validate tests which are given by default file (in packages/PIPS/validation)")
	parser.add_option("--p4a", action="store_true", dest="par4all", help = "Validate tests which are given by par4all_validation.txt (which must be previously created in par4all/src/validation)")
	parser.add_option("--diff", action="store_true", dest="diff", help = "List tests that are done with pips option but not with p4a option")
	parser.add_option("--dir", action="store_true", dest="dir", help = "Validate tests which are located in packages/PIPS/validation/directory_name")
	parser.add_option("--test", action="store_true", dest="test", help = "Validate tests given in argument")
	(options, args) = parser.parse_args()

	#set all locale categories to C (English), to make the test results consistent to match
	#the references. This is needed because the test references have been defined using 
	#this environment variable, so some shell commands used in the tests
	#such as 'cat */*.c' will be done in the same order as the references
	os.putenv('LC_ALL', 'C')

	if options.pips:
		vc = ValidationClass().valid_pips()
		print('Result of the tests are in pips_log.txt')

	elif options.par4all:
		vc = ValidationClass().valid_par4all()
		print('Result of the tests are in p4a_log.txt')

	elif options.diff:
		vc = ValidationClass().diff()
		print('Tests which are not done by --p4a options are into diff.txt file')

	elif options.dir:
		if (len(args) == 0):
			print("You must enter the name of the directories you want to test")
			exit()

		vc = ValidationClass().valid_dir(args)
		print('Result of the tests are in directory_log.txt')

	elif options.test:
		if (len(args) == 0):
			print("You must enter the name of the tests you want to test")
			exit()

		vc = ValidationClass().valid_test(args)

	else:
		output = commands.getoutput("python p4a_validate_class.py -h")
		print(output)

	os.chdir(os.getcwd())

# If this programm is independent it is executed:
if __name__ == "__main__":
    main()
