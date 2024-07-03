"""Key generator

This python script traverses a path of csv files containing AccProf profiling
results of an application. It reads the contents of those files and adds them
to a dataset. Then it generates a key list for every function instance that
contains a key and the amount of LS bits that were discarded (right-shifted)
for every event. We need this number for the LLVM pass to know how many bits to
right-shift on the events counted during execution
"""

import csv
import sys
import os
import pprint

EXPERIMENT_FILE_OFFSET = 2
A53_MAX_COUNTERS_NUM = 6

functions_num = 0
events_num = 0
execs_num = 0
files = []


def main():
	path = sys.argv[1]
	if path[-1] != '/':
		path += "/"
	init_globals_and_path(path)
	dataset = init_dataset(path)
	key_list = create_key_list(dataset)
	write_key_list_to_file(key_list, path)


def init_globals_and_path(path: str):
	"""
	Goes to the path specified and (1) deletes previously generated keys.csv if
	it exists (2) uses the evaluation files to populate functions_num,
	events_num and execs_num used to later traverse the dataset. This function
	assumes the directory contains only the csv files containing the event
	counts and maybe a previously generated file with the keys

	Number of executions is equal to the number of files
	Number of functions is the number of lines of a file excluding the header
	Number of events is the number of available counters (chip dependent) and
	the cycle counter.

	Parameters
	----------
	path: str
		The path to evaluation files and where key file will be created
	"""
	global functions_num, events_num, execs_num, files
	try:
		os.remove(path + 'keys.csv')
	except FileNotFoundError:
		pass
	files = os.listdir(path)
	execs_num = len(files) # each file is a different execution
	csv_file = open(path + files[0], 'r')
	functions_num = len(csv_file.readlines()) - 1
	csv_file.close()
	events_num = A53_MAX_COUNTERS_NUM + 1


def init_dataset(path: str) -> list[list[list[int]]]:
	"""
	Takes a path where evaluation files exist, parses them and fills a 3D
	dataset. Then returns the dataset. Each 2D matrix(1st dimension)
	corresponds to a function instance, each row in a matrix (2nd dimension)
	corresponds to a function event, each column (3rd dimension) corresponds
	to an execution instance, and each element is an event count

	Parameters
	----------
	path: str
		The path to evaluation files and where key file will be created

	Returns
	-------
	list[list[list[int]]]
		The dataset populated with the file contents
	"""
	dataset = [[[0 for _ in range(execs_num)] for _ in range(events_num)] for _ in range(functions_num)]
	exec = 0
	for file in files:
		csv_file = open(path + file, 'r')
		csv_reader = csv.reader(csv_file, delimiter=',')
		_ = next(csv_reader)
		func = 0
		for row in csv_reader:
			for event in range(events_num):
				dataset[func][event][exec] = int(row[event + EXPERIMENT_FILE_OFFSET])
			func +=1
		exec += 1
		csv_file.close()
	return dataset


def create_key_list(dataset: list[list[list[int]]]) -> list[list[int]]:
	"""
	Takes a dataset containing event counts, and creates a 2D key list. Each
	row corresponds to a function and each column except the last, corresponds
	to an event. The last column correponds to the key for each function. The
	key is made by concatenating the stable bits of every event of that
	function. The stable bits of an event are the MS bis that have no
	difference between all event counts of that event. Each other column
	contains the number of unstable LS bits for each event

	Parameters
	----------
	dataset: list[list[list[int]]]
		The dataset containing the event counts

	Returns
	-------
	list[list[int]]
		The key list containing the keys and amount of bits shifted of events
	"""
	key_list = [[0 for _ in range(events_num + 1)] for _ in range(functions_num)]
	for func in range(functions_num): # Every function instance will generate 1 key
		total_stable_bits_num = 0
		total_stable_bits = 0
		pprint.pprint(f'function {func}:')
		for event in range(events_num):
			max_event_count = max(dataset[func][event])
			if max_event_count == 0:
				continue
			min_event_count = min(dataset[func][event])
			xor_event_count = max_event_count ^ min_event_count
			max_event_count_msb_pos = get_msb_pos(max_event_count)
			xor_event_count_msb_pos = get_msb_pos(xor_event_count)
			event_unstable_bits_num = xor_event_count_msb_pos + 1
			event_stable_bits = max_event_count >> event_unstable_bits_num
			key_list[func][event] = event_unstable_bits_num;
			print(f'bits shifted for event {event}: {event_unstable_bits_num}')
			event_stable_bits_lsl = event_stable_bits << total_stable_bits_num
			total_stable_bits = total_stable_bits | event_stable_bits_lsl
			total_stable_bits_num += max_event_count_msb_pos - xor_event_count_msb_pos
			pprint.pprint(f'    {bin(max_event_count)}:{bin(min_event_count)}:{bin(xor_event_count)}:{bin(event_stable_bits)}')
		key_list[func][events_num] = total_stable_bits;
		pprint.pprint(f'key (bin): {bin(total_stable_bits)}:')
		pprint.pprint(f'key (dec): {total_stable_bits}:')
	return key_list


def write_key_list_to_file(key_list: list[list[int]], path: str):
	"""
	Goes to the path specified and (1) writes the contents of key_list to a
	csv file

	Parameters
	----------
	key_list: list[list[int]]
		The key list to write to the csv file
	path: str
		The path to evaluation files and where key file will be created
	"""
	csv_file = open(path + 'keys.csv', 'w+')
	csv_writer = csv.writer(csv_file)
	csv_writer.writerow(['event0-shift','event1-shift','event2-shift','event3-shift','event4-shift','event5-shift','event6-shift','function-key'])
	for func in range(functions_num):
		csv_writer.writerow(key_list[func])
	csv_file.close()


def get_msb_pos(n: int) -> int:
	"""
	Returns the msb position of n or -1 if n = 0. e.g. for n = 5 = 0b101
	returns 2

	Parameters
	----------
	n: int
		The number which its msb postion will be retuned
	"""
	if n == 0:
		return -1
	ms_set_bit_pos = 0
	while n != 1:
		ms_set_bit_pos += 1
		n = n >> 1;
	return ms_set_bit_pos


if __name__ == "__main__":
	main()
