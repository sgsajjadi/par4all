#! /usr/bin/python

# -*- coding: utf-8 -*-

"""
Validation utility for Par4All

Add object oriented organization above PIPS validation.

Introduce the concept of validation class.
"""
import os, string, re, optparse, glob, shutil, stat, multiprocessing, subprocess, datetime, signal

# Init variables
p4a_root = ''

for root, subfolders, files in os.walk(os.environ.get("PWD")):
	if os.path.exists(root+'/p4a_validate_class.py'):
		p4a_root = root

	if (not p4a_root):
		print ('You need to define P4A_ROOT environment variable')
		exit()

par4ll_validation_dir = p4a_root+'/../../packages/PIPS/validation/'

extension = ['.c','.F','.f','.f90','.f95']

# warning and failed status
warning = ['skipped','timeout']
failed = ['changed','failed']

# get default architecture and tpips/pips
arch = ''
for root, subfolders, files in os.walk(par4ll_validation_dir+'/..'):
	if os.path.exists(root+'/arch.sh'):
		arch = subprocess.Popen([root+'/arch.sh'], stdout=subprocess.PIPE).communicate()[0]

# Timeout
timeout = 600 # Time in second
timeout_value = 203 # Value of timeout status

# Multiprocessing
nb_cpu = multiprocessing.cpu_count()

# List of directories that will be tested
dir_list = list()

# Alarm class (timeout)
class Alarm(Exception):
  pass

### Raise Alarm ###
  def alarm_handler(self,signum,frame):
	raise Alarm

# Functions ##################
### Write in log ###
def write_log(status,log_file,test_name_path):
	# Lock to have only one process who write to log_file
	lock = multiprocessing.Lock()
	lock.acquire()
	# Write status
	file_result = open(p4a_root+'/'+log_file,'a')
	file_result.write ('%s: %s\n' % (status,test_name_path.replace(par4ll_validation_dir,'')))
	file_result.close()
	# Unlock
	lock.release()

### run test ###
def process_timeout(command,status,queue_err,queue_output,shell_value,err_pipe):
	# Process for tpips2 file
	if err_pipe != 'STDOUT':
		process = subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,shell=shell_value)
	else:
		process = subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,shell=shell_value)

	# Alarm for timeout
	signal.signal(signal.SIGALRM, Alarm().alarm_handler)
	signal.alarm(timeout)
	communicate=['','']
	try:
		communicate = process.communicate()
		signal.alarm(0)  # reset the alarm
	except Alarm:
		# kill process after timeout
		process.kill()
		status.value = timeout_value

	# Output of the test
	queue_output.put(communicate[0])

	# Error of the test
	queue_err.put(communicate[1])

	if status.value != timeout_value:
		# Status
		status.value = process.returncode

###### Function that kills a process after a timeout ######
def run_process(command,shell_value,err_pipe):
	status = multiprocessing.Value('i',0)
	output = ''
	err = ''

	# Queues
	queue_output = multiprocessing.Queue()
	queue_err = multiprocessing.Queue()

	# Create and run process
	process_timeout(command,status,queue_err,queue_output,shell_value,err_pipe)

	# Output and error
	output = queue_output.get()
	err = queue_err.get()

	queue_output.close()
	queue_err.close()

	return status.value,output,err

#### Command to test ###
def command_test(directory_test_path,test_name_path,err_file_path,test_file_path):
	# by default error is sent to the PIPE
	err_pipe = "PIPE"
	if (os.path.isfile(test_name_path+".test")):
		command = [test_name_path+".test",""]
		(int_status,output,err) = run_process(command,True,err_pipe)

	elif (os.path.isfile(test_name_path+".tpips")):
		command = ["tpips",test_name_path+".tpips"]
		(int_status,output,err) = run_process(command,False,err_pipe)

	elif (os.path.isfile(test_name_path+".tpips2")):
		command = ["tpips",test_name_path+".tpips2"]
		err_pipe = "STDOUT"
		(int_status,output,err) = run_process(command,False,err_pipe)

	elif (os.path.isfile(test_name_path+".py")):
		command = ["python",os.path.basename(test_name_path)+".py"]
		(int_status,output,err) = run_process(command,False,err_pipe)

	elif (os.path.isfile(directory_test_path+"/default_tpips")):
		# Change flag for default_tpips
		os.chmod(directory_test_path+"/default_tpips", stat.S_IEXEC | stat.S_IREAD | stat.S_IWRITE)
		command = "FILE="+test_file_path+" WSPACE="+os.path.basename(test_name_path)+" tpips "+directory_test_path+"/default_tpips"
		# Launch process
		(int_status,output,err) = run_process(command,True,err_pipe)

	elif (os.path.isfile(directory_test_path+"/default_test")):
		# Change flag for default_test
		os.chmod(directory_test_path+"/default_test", stat.S_IEXEC | stat.S_IREAD | stat.S_IWRITE)
		# test_name=file
		# upper=FILE
		upper = os.path.basename(test_name_path).upper()
		command = "FILE="+test_file_path+" WSPACE="+os.path.basename(test_name_path)+" NAME="+upper+" "+directory_test_path+"/default_test 2>"+err_file_path
		(int_status,output,err) = run_process(command,True,err_pipe)

	elif (os.path.isfile(directory_test_path+"/default_pyps.py")):
		# Change flag for default_pyps.py
		os.chmod(directory_test_path+"/default_pyps.py", stat.S_IEXEC | stat.S_IREAD | stat.S_IWRITE)
		command = "FILE="+test_file_path+" WSPACE="+os.path.basename(test_name_path)+" python "+directory_test_path+"/default_pyps.py 2>"+err_file_path
		(int_status,output,err) = run_process(command,True,err_pipe)

	# Pips in command line
	else:
		subprocess.Popen(["Delete",os.path.basename(test_name_path)])

		command = ["Init","-f",test_file_path,"-d",os.path.basename(test_name_path)]
		(int_status,output,err)=run_process(command,False,err_pipe)
	
		if int_status == 0:
			command = "while read module ; do Display -m  $module -w "+os.path.basename(test_name_path)+" ; done < "+os.path.basename(test_name_path)+".database/modules"
			(int_status, output_display,err_display)=run_process(command,True,err_pipe)

		subprocess.Popen(["Delete",os.path.basename(test_name_path)])

	if err != None:
		err_file_path_h = open(err_file_path, 'w')
		err_file_path_h.write(err)
		err_file_path_h.close()

	return (int_status,output)

#### Function which run tests and save result on result_log ######
def test_par4all(directory_test_path,test_file_path,log_file):
	# .result directory of the test to compare results
	test_file_path = test_file_path.strip('\n')
	(test_name_path, ext) = os.path.splitext(test_file_path)
	test_result_path = test_name_path + '.result'

	#output ref of the test
	test_ref_path = test_result_path + '/test'

	status = 'succeeded'

	#check that .result and reference of the test are present. If not, status is "skipped" 
	if (os.path.isdir(test_result_path) != True or (os.path.isfile(test_ref_path) != True and os.path.isfile(test_ref_path+'.'+arch) != True)):
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

		for filename in glob.glob(par4ll_validation_dir+'/*.database') :
			shutil.rmtree(filename,ignore_errors=True) 

		# test
		(int_status,output) = command_test(directory_test_path,test_name_path,err_file_path,test_file_path)

		# filter out absolute path anyway, there may be some because of
		# cpp or other stuff run by pips, even if relative path names are used.
		output = output.replace(directory_test_path,'.')

		if (os.path.isfile(err_file_path) == True):
			# copy error file on RESULT directories of par4all validation
			new_err = err_file_path.replace(par4ll_validation_dir,'')
			shutil.move(err_file_path,p4a_root+'/RESULT/'+new_err.replace('/','_'))#+'.err')

		if (int_status == timeout_value):
			status = 'timeout'
		elif(int_status != 0):
			status = "failed"
		else:
			if(os.path.isfile(test_ref_path+'.'+arch) == True):
				ref_path = test_ref_path+'.'+arch
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

				sub_process_ref = subprocess.Popen([test_result_path + "/test_and_out.filter",reference_filtered_path], stdout=subprocess.PIPE)
				reference_filtered = sub_process_ref.communicate()[0]
				int_status = sub_process_ref.returncode

				sub_process_out = subprocess.Popen([test_result_path + "/test_and_out.filter",output_file_filter_path], stdout=subprocess.PIPE)
				out_filtered = sub_process_out.communicate()[0]
				int_status = sub_process_out.returncode

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

					sub_process_ref = subprocess.Popen([test_result_path + "/test_and_out.filter",reference_filtered_path], stdout=subprocess.PIPE)
					reference_filtered = sub_process_ref.communicate()[0]
					int_status = sub_process_ref.returncode

					# Write new "test reference" filtered
					reference_filter_path = ref_path + '.filtered'
					reference_filter_h = open(reference_filter_path,'w')
					reference_filter_h.write ('%s' % (reference_filtered))
					reference_filter_h.close()
					ref_is_filtered=1

				# apply the user define filter on the output
				if (os.path.isfile(test_result_path + "/out.filter")):
					os.chmod(test_result_path + "/out.filter", stat.S_IEXEC | stat.S_IREAD | stat.S_IWRITE)

					sub_process_out = subprocess.Popen([test_result_path + "/test_and_out.filter",output_file_filter_path], stdout=subprocess.PIPE)
					out_filtered = sub_process_out.communicate()[0]
					int_status = sub_process_out.returncode

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
	write_log(status,log_file,test_file_path)

	# Return to validation Par4All
	os.chdir(p4a_root)
	return status

###### Validate only test what we want ######
def valid_par4all():
	os.chdir(p4a_root)
	if os.path.isfile('par4all_validation.txt'):
		f = open("par4all_validation.txt")
	else:
		print ('No par4all_validation.txt file in %s. Create one before launch validation par4all'%(p4a_root))
		exit()

	# Create directory for result
	if (os.path.isdir("RESULT") == True):
		shutil.rmtree("RESULT", ignore_errors=True)
	os.mkdir("RESULT")

	if os.path.isfile('p4a_log.txt'):
		os.remove("p4a_log.txt")

	test_list = list()
	log_file = "p4a_log.txt"
	# Open the file where par4all tests are:
	for line in f:
		# Check line is not empty
		if len(line.strip('\n').replace(' ','')) != 0 :
			# In case of the test is written like Directory_test\test.f instead os Directory_test/test.f
			line = line.replace('\\','/')
			# delete .f, .c and .tpips of the file name
			(root, ext) = os.path.splitext(line)

			# split to have: link to folder of the test and name of the test
			directory=root.split("/")
			directory_test = par4ll_validation_dir

			for j in range(0,len(directory)-1):
				directory_test = directory_test+'/'+directory[j]

			# print (('# Considering %s')%(line.strip('\n')))

			ext = ext.strip('\n').strip(' ')

			file_tested = (par4ll_validation_dir+line).strip('\n')

			if(ext in extension):
				if (os.path.isdir(directory_test) != True ):
					print('%s is not a directory into packages/PIPS/validation'%(directory_test.replace(par4ll_validation_dir+'/','')))
				elif (os.path.isfile(par4ll_validation_dir+line.strip('\n').strip(' ')) != True):
					print('%s is not a file into packages/PIPS/validation/'%(line.strip('\n')))
				else:
					# Run test
					# test_par4all(directory_test,par4ll_validation_dir+line,'p4a_log.txt')
					test_list.append(file_tested)
			elif (ext == '.result'):
					test_name_list = search_file(file_tested.replace('.result',''))
					for test_name_path in test_name_list:
						test_list.append(file_tested)
			else:
				print ("To test %s, use an extension like %s\n"%(line.strip('\n'),extension))

	f.close()

	# Launch test in multithread
	multithread_test(test_list,log_file)

	f_log=open(log_file)
	for line in f_log:
		print line.strip('\n')
	f_log.close()

	count_failed_warn(log_file)

#### Multiprocessing test ####
def multi_test(result_dir,directory_test,log_file):
	test_name_list = search_file(result_dir.replace('.result',''))
	for test_name_path in test_name_list:
		test_par4all(directory_test,test_name_path,log_file)

### .result test to test ####
def result_test(directory_test,resultdir_list,log_file):
	for resultdir in resultdir_list:
		multi_test(resultdir,directory_test,log_file)

#### Directory to test - Recursive to enter in subdirectories ####
def recursive_dir_test(dirlist,log_file,new_dir_list):
	# Check that list is not empty, so there is directory to test
	if (len(dirlist) != 0):
		i = 0
		# List of subdirectories to test
		dir_sublist = list()

		# Browse directory
		for i in range(0,len(dirlist)):
			resultdir_list = list()
			directory_test = dirlist[i]

			print (('# Considering %s')%(directory_test.replace(par4ll_validation_dir,'').strip('\n')))

			# List file/directories to test, subdirectories and skipped status (no correspondig .result folder)
			listing = os.listdir(directory_test)
			for dirfile in listing:
				dir_sublist,resultdir_list = subdir_list_test(directory_test,log_file,resultdir_list,dirfile,dir_sublist)

			# Test
			result_test(directory_test,resultdir_list,log_file)
			print (('# %s Finished')%(directory_test.replace(par4ll_validation_dir,'').strip('\n')))

		# Lock to have only one process who add dir to test
		lock = multiprocessing.Lock()
		lock.acquire()
		for dir in dir_sublist:
			new_dir_list.append(dir)
		lock.release()

### Return number of failed, warning and test ####
def count_failed_warn(log_file):
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
		for i in range(0,len(warning)):
			nb_warning = nb_warning + int(log_file_str.count(warning[i]+':'))

		# Number of failed
		i = 0
		for i in range(0,len(failed)):
			nb_failed = nb_failed + int(log_file_str.count(failed[i]+':'))

		log_file_h.close()

		if ('diff.txt' != log_file):
			print('%s failed and %s warning %s in %s tests'%(nb_failed,nb_warning,warning,nb_test))
		else:
			print('%i tests are not done by --p4a options or their status is "skipped"'%(nb_test))

### Check subdirectories to test and skipped status (no correspondig .result folder) ####
def subdir_list_test(directory_test,log_file,resultdir_list,dirfile,dir_sublist):
	(root, ext) = os.path.splitext(dirfile)
	# Is it a directory?
	if os.path.isdir(directory_test+'/'+dirfile):
		if (ext == '.result'):
			resultdir_list.append(directory_test+'/'+dirfile)
		elif (ext == '.sub'):
			dir_sublist.append(directory_test+'/'+dirfile)
		else:
			# Is it in the makefile?
			for line in open(directory_test+"/Makefile"):
				if ((dirfile in line) and ("D.sub" in line)) :
					dir_sublist.append(directory_test+'/'+dirfile)
	# This is a file
	else:
		if ext in extension:
			test_result_path = directory_test+'/'+root + '.result'

			#output ref of the test
			test_ref_path = test_result_path + '/test'

			#check that .result and reference of the test are present. If not, status is "skipped" 
			if (os.path.isdir(test_result_path) != True or (os.path.isfile(test_ref_path) != True and os.path.isfile(test_ref_path+'.'+arch) != True)):
				write_log('skipped',log_file,directory_test+'/'+dirfile)

	return dir_sublist,resultdir_list

### List tests in directories and subdirectories and check that it's not present in par4all_validation.txt ####
def recursive_list_test(dir_list,par4all_string):
	# Check that list is not empty, so there is directory to test
	if (len(dir_list) != 0):
		i = 0

		# Browse directory
		for i in range(0,len(dir_list)):
			directory_test = dir_list[i]
			# List of subdirectories to test
			dir_sublist = list()

			# Check subdirectories to test and skipped status (no correspondig .result folder)
			resultdir_list = list()
			listing = os.listdir(directory_test)
			for dirfile in listing:
				dir_sublist,resultdir_list = subdir_list_test(directory_test,"diff.txt",resultdir_list,dirfile,dir_sublist)

			for result_dir in resultdir_list:
				# Search a correspondig file of .result folder
				test_name_list = search_file(result_dir.replace('.result',''))
				for test_name_path in test_name_list:
					# Test is find. Check that it is present into par4all_validation.txt
					test_string = test_name_path.replace(par4ll_validation_dir,'').strip('\n')

					# Is test is in par4all_validation.txt ?
					if int(par4all_string.count(test_string)) == 0:
						diff_test_h = open('diff.txt','a')
						diff_test_h.write(test_name_path.replace(par4ll_validation_dir,'')+'\n')
						diff_test_h.close()

		recursive_list_test(dir_sublist,par4all_string)

### Find file (c, fortran, etc...) of the corresponding .result test ###
def search_file(filename):
	file_found = 0
	files = list()

	# List all tests file with corresponding .result folder
	for ext in extension:
		if os.path.exists(filename+ext):
			files.append(filename+ext)
			file_found = 1

	if file_found == 0:
		files.append(filename+'.result')

	return files

### Directories and subdirectories to test ###
def dir_subdir_test(dir_list,log_file):
	# list of launched process
	process_list = list()
	i = 0

	while i < len(dir_list):
		# Create temporary dir_list. This list will be tested after.
		dir_list_temp = list()
		for j in range(0,nb_cpu):
			if i < len(dir_list):
				dir_list_temp.append(dir_list[i])
				i = i+1

		# Create a manager to update dir_list
		manager = multiprocessing.Manager()
		new_dir_list = manager.list()

		# Multithread test
		for dir in dir_list_temp:
			process = multiprocessing.Process(target=recursive_dir_test, args=([dir],log_file,new_dir_list))
			process.start()
			process_list.append(process)

		# Wait for all threads to complete
		for thread in process_list:
			thread.join()

		# add subdirectories to the list of test
		dir_list += new_dir_list

### Multithread test for par4all and test options ###
def multithread_test(test_list,log_file):
	# list of launched process
	process_list = list()
	i = 0

	while i < len(test_list):
		# Create temporary test_list. This list will be tested after.
		test_list_temp = list()
		for j in range(0,nb_cpu):
			if i < len(test_list):
				test_list_temp.append(test_list[i])
				i = i+1

		# Multithread test
		for test in test_list_temp:
			test_array=test.split("/")

			# Directory of the test
			directory_test = ''
			for j in range(0,len(test_array)-1):
				directory_test = directory_test+'/'+test_array[j]
				
			print (('# Considering %s')%(test.replace(par4ll_validation_dir,'').strip('\n')))

			process = multiprocessing.Process(target=test_par4all,args=(directory_test,test,log_file))
			process.start()
			process_list.append(process)

		# Wait for all threads to complete
		for thread in process_list:
			thread.join()

###### Validate all tests (done by "default" file) ######
def valid_pips():
	global dir_list
	os.chdir(p4a_root)

	# Create directory for result
	if (os.path.isdir("RESULT") == True):
		shutil.rmtree("RESULT", ignore_errors=True)
	os.mkdir("RESULT")

	default_file_path = par4ll_validation_dir+"defaults"

	default_file = open(default_file_path)

	if os.path.isfile('pips_log.txt'):
		os.remove("pips_log.txt")

	# List all directories that we must test
	for line in default_file:
		if (not re.match('#',line)):
			line  = line.strip('\n')
			dir_list.append(par4ll_validation_dir+line)

	# Multithreading test
	if len(dir_list) != 0:
		dir_subdir_test(dir_list,'pips_log.txt')
	else:
		print "No folder to test according to"+par4ll_validation_dir+"defaults"

	count_failed_warn('pips_log.txt')
	default_file.close()

###### Diff between p4a and pips options ######
def diff():
	os.chdir(p4a_root)
	# Read default file to build a file with all tests
	default_file = open(par4ll_validation_dir+"defaults")

	diff_file = open('diff.txt','w')
	diff_file.close()

	if os.path.isfile(p4a_root+'/par4all_validation.txt'):
		par4all_h = open(p4a_root+"/par4all_validation.txt")
		par4all_string = par4all_h.read()
		par4all_string = par4all_string.replace('\\','/').strip('\n')
		par4all_h.close()
	else:
		par4all_string = ''

	# Parse all tests done by default file in pips validation and build a file with all tests which are not written in par4all_validation.txt
	for line in default_file:
		if (not re.match('#',line)):
			line  = line.strip('\n')
			dir_list = [par4ll_validation_dir+line]
			recursive_list_test(dir_list,par4all_string)

	count_failed_warn('diff.txt')

###### Validate all tests of a specific directory ################
def valid_dir(arg_dir):
	global dir_list
	os.chdir(p4a_root)
	if os.path.isfile('directory_log.txt'):
		os.remove("directory_log.txt")

	# Create directory for result
	if (os.path.isdir("RESULT") == True):
		shutil.rmtree("RESULT", ignore_errors=True)
	os.mkdir("RESULT")

	# Build directory list to test
	for directory_name in arg_dir:
		# Is it a valid directory?
		if (os.path.isdir(par4ll_validation_dir+directory_name) != True):
			print ("%s does not exist or it's not a repository"%(directory_name))
		else:
			dir_list.append(par4ll_validation_dir+directory_name)

	# Multithreading test
	if len(dir_list) != 0:
		dir_subdir_test(dir_list,"directory_log.txt")
	else:
		print ('None valid folder to test in %s'%(arg_dir))

	count_failed_warn('directory_log.txt')

###### Validate all desired tests ################
def valid_test(arg_test):
	os.chdir(p4a_root)
	# Create directory for result
	if (os.path.isdir("RESULT") == True):
		shutil.rmtree("RESULT", ignore_errors=True)
	os.mkdir("RESULT")

	if os.path.isfile('test_log.txt'):
		os.remove("test_log.txt")
	log_file = 'test_log.txt'

	test_list = list()

	#read the tests
	for i in range(0,len(arg_test)):
		test_array=arg_test[i].split("/")

		# Directory of the test
		directory_test = par4ll_validation_dir
		for j in range(0,len(test_array)-1):
			directory_test = directory_test+'/'+test_array[j]

		# File to test
		file_tested = directory_test+'/'+test_array[len(test_array)-1]
		(root, ext) = os.path.splitext(test_array[len(test_array)-1])

		# Check that directory and test exist
		if (os.path.isdir(directory_test) != True):
			print('%s is not a directory into packages/PIPS/validation'%(directory_test.replace(par4ll_validation_dir+'/','')))
		elif (os.path.isfile(file_tested) != True and ext != '.result'):
			print('%s is not a file into packages/PIPS/validation/%s'%(arg_test[i],directory_test.replace(par4ll_validation_dir,'')))
		else:
			# Check that extension of the file is OK
			if(ext in extension):
				test_list.append(file_tested)
				# Test
				# status = test_par4all(directory_test, file_tested,log_file)
				# print('%s : %s'%(arg_test[i],status))
			elif (ext == '.result'):
				test_name_list = search_file(file_tested.replace('.result',''))
				for test_name_path in test_name_list:
					test_list.append(file_tested)
					# status = test_par4all(directory_test,test_name_path,log_file)
					# print('%s : %s'%(test_name_path.replace(par4ll_validation_dir+'/',''),status))
			else:
				print('%s : Not done (extension must be %s or a valid .result folder)'%(arg_test[i],extension))

	# Launch test in multithread
	multithread_test(test_list,log_file)

	f=open(log_file)
	for line in f:
		print line.strip('\n')
	f.close()

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
		vc = valid_pips()
		print('Result of the tests are in pips_log.txt')

	elif options.par4all:
		vc = valid_par4all()
		print('Result of the tests are in p4a_log.txt')

	elif options.diff:
		vc = diff()
		print('Tests which are not done by --p4a options are into diff.txt file')

	elif options.dir:
		if (len(args) == 0):
			print("You must enter the name of the directories you want to test")
			exit()

		vc = valid_dir(args)
		print('Result of the tests are in directory_log.txt')

	elif options.test:
		if (len(args) == 0):
			print("You must enter the name of the tests you want to test")
			exit()

		vc = valid_test(args)

	else:
		# Help
		print(subprocess.check_output(["python p4a_validate_class.py","-h"]))

	os.chdir(os.getcwd())

# If this programm is independent it is executed:
if __name__ == "__main__":
    main()

