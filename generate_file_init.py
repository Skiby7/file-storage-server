import sys
import os
import random
import string

def string_generator(size, chars=string.ascii_uppercase + string.ascii_lowercase + string.digits + ' '):
	return ''.join(random.choice(chars) for _ in range(size))


path = input("Where do you want to save the files? ")

try:
	os.chdir(path)
except:
	os.mkdir(path)
	os.chdir(path)

try:
	os.chdir('small_files')
except:
	os.mkdir('small_files')
	os.chdir('small_files')

print()
for i in range(100):
	f = open("small_" + str(i) + '.dat', "w+")
	f.write(string_generator(1024))
	f.close()
	print("\033[AGenerated " + str(i+1) + " small files")
os.chdir('..')
os.system('chmod 777 -R small_files')

print()
try:
	os.chdir('medium_files')
except:
	os.mkdir('medium_files')
	os.chdir('medium_files')
print()
for i in range(10):
	f = open("medium_" + str(i) + '.dat', "w+")
	f.write(string_generator(10000000))
	f.close()
	print("\033[AGenerated " + str(i+1) + " medium files")
os.chdir('..')
os.system('chmod 777 -R medium_files')

print()
try:
	os.chdir('large_files')
except:
	os.mkdir('large_files')
	os.chdir('large_files')
print()
for i in range(5):
	f = open("large_" + str(i) + '.dat', "w+")
	f.write(string_generator(50000000))
	f.close()
	print("\033[AGenerated " + str(i+1) + " large files")
os.chdir('..')
os.system('chmod 777 -R large_files')

print()


print("Done")
