import csv
import sys
import os
import pprint

class Prof_func:
	def __init__(self, name):
		self.name = name
		self.cycles_list = []
		self.event_lists = [[], [], [], [], [], []]

	def __repr__(self):
		return (
			"% s: cycle size: % s, ev0 size: % s" %
			(self.name, len(self.cycles_list), len(self.event_lists[0]))
		)

KEYS_IDX = 0
EVENT_SHIFTS_IDX = 1

KEYS_FILE = "/keys.txt"
EVENT_SHIFTS_FILE = "/event-shifts.txt"
CWD = os.path.dirname(os.path.realpath(__file__));
VARIANCE_FILE_PATH = CWD + "/../accProfPass/variances.txt"
FUNCTIONS_FILE_PATH = CWD + "/../accProfPass/functions.txt"

# hold profiled functions in a dictionary. the id (as string) is used as the
# key. in later versions more attributes can be concatenated with the id for a
# more complex key string
func_names = {}
prof_funcs = {}
events_names = []
events_variances = []
variances_i = 0

def main():
	global variances_i
	csv_file = sys.argv[1]
	out_path = os.path.dirname(os.path.realpath(csv_file));
	variances_i = int(sys.argv[2])
	print(csv_file, out_path)

	init_globals(out_path, csv_file)
	print("events:", len(events_names), ":", events_names)
	print("event variances: ", events_variances)
	print("functions:")
	for key, val in func_names.items():
		print("\t", key, val)
	prof_funcs = init_func_dict(csv_file)
	lists = create_lists(prof_funcs)
	print(lists)
	write_list_to_file(lists[KEYS_IDX], out_path, "/keys-"+str(variances_i)+".txt")
	write_list_to_file(lists[KEYS_IDX], out_path, KEYS_FILE)
	write_list_to_file(lists[EVENT_SHIFTS_IDX], out_path, EVENT_SHIFTS_FILE)
	write_list_to_file(lists[EVENT_SHIFTS_IDX], out_path, "/event-shifts-"+str(variances_i)+".txt")
	write_prof_funcs_to_files(out_path, prof_funcs)

def init_globals(path: str, file: str):
	"""
	Goes to the path specified and (1) deletes previously generated keys.txt if
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
	global func_names, events_names, events_variances, variances_i
	try:
		os.remove(path + KEYS_FILE)
	except FileNotFoundError:
		pass
	try:
		os.remove(path + EVENT_SHIFTS_FILE)
	except FileNotFoundError:
		pass

	# read functions file to init dictionary with names(vals) and ids(keys)
	with open(FUNCTIONS_FILE_PATH, 'r') as functions_file:
		for line in functions_file:
			val, key = line.split()
			func_names[key] = val

	# read only the first line of the file to get the even names and number
	with open(file, 'r') as csv_file:
		csv_reader = csv.reader(csv_file, delimiter=',')
		events_names = next(csv_reader)[3:]
		events_names = list(filter(None, events_names)) # remove empty strings

	# read line from variances file to know how many bits to loosen
	with open(VARIANCE_FILE_PATH, 'r') as events_file:
		for i, line in enumerate(events_file):
			if i == variances_i:
				events_variances = [int(n) for n in line.split()]

def init_func_dict(file: str) -> dict[str, Prof_func]:
	"""
	Takes a path to a csv file and creates a dictionary made of objects of
	class Prof_func, corresponding to profiled functions. The dictionary is
	filled with the data read from the input file.

	Parameters
	----------
	file: str
		The input csv file to fill the profiled function dictionary

	Returns
	-------
	dict[str, Prof_func]
		The profile function dictionary
	"""
	global func_names, events_names, events_variances, variances_i

	prof_funcs = {}
	with open(file, 'r') as csv_file:
		csv_reader = csv.reader(csv_file, delimiter=',')
		_ = next(csv_reader)
		for row in csv_reader:
			func_id = row[0]
			# create or retrieve existing function based on the id
			prof_func = prof_funcs.get(func_id)
			if not prof_func:
				prof_func = Prof_func(func_names[func_id])
				prof_funcs[func_id] = prof_func
			# add current row or cycles and events to function
			prof_func.cycles_list.append(int(row[2]))
			for i in range(len(events_names)):
				prof_func.event_lists[i].append(int(row[3+i]))
	return prof_funcs

def create_lists(prof_funcs: dict[str, Prof_func]) -> list[list[int]]:
	global func_names, events_names, events_variances, variances_i

	key_list = []
	event_shifts_list = [[0 for _ in range(len(events_names) + 1)] for _ in range(len(func_names))]
	func_i = 0
	for fid, prof_func in prof_funcs.items():
		total_stable_bits_num = 0
		total_stable_bits = 0
		for event in range(0, len(events_names)): # 1 extra col for func id
			max_event_count = max(prof_func.event_lists[event])
			if max_event_count == 0:
				continue
			min_event_count = min(prof_func.event_lists[event])
			xor_event_count = max_event_count ^ min_event_count
			print(fid, events_names[event], max_event_count, min_event_count)
			max_event_count_msb_pos = get_msb_pos(max_event_count)
			xor_event_count_msb_pos = get_msb_pos(xor_event_count)
			event_unstable_bits_num = xor_event_count_msb_pos + 1
			event_stable_bits = max_event_count >> event_unstable_bits_num + events_variances[event]
			event_shifts_list[func_i][event] = event_unstable_bits_num + events_variances[event];
			event_stable_bits_lsl = event_stable_bits << total_stable_bits_num
			total_stable_bits = total_stable_bits | event_stable_bits_lsl
			event_stable_bits_num = max_event_count_msb_pos - xor_event_count_msb_pos
			if event_stable_bits_num - events_variances[event] < 0:
				event_stable_bits_num = 0
			else:
				event_stable_bits_num -= events_variances[event]
			total_stable_bits_num += event_stable_bits_num
		event_shifts_list[func_i][len(events_names)] = fid
		key_list.append(hex(total_stable_bits));
		func_i += 1
	return [key_list, event_shifts_list]

def write_prof_funcs_to_files(path: str, prof_funcs: dict[str, Prof_func]):
	for fid, prof_func in prof_funcs.items():
		with open(path + "/" + prof_func.name + ".txt", 'w') as out_file:
			writer = csv.writer(out_file)
			writer.writerow(prof_func.cycles_list)
			for event_i in range(0, len(events_names)):
				writer.writerow(prof_func.event_lists[event_i])


def write_list_to_file(alist: list[any], path: str, output_file: str):
	"""
	Goes to the path specified and  writes the contents of alist to output_file

	Parameters
	----------
	alist: list[Any]
		The list to write to the output file
	path: str
		The path to evaluation files and where the output file will be created
	filename: str
		The output file to write the list contents
	"""
	file = open(path + output_file, 'w+')
	for func in range(len(func_names)):
		if type(alist[func]) == type([]):
			line = ",".join(str(x) for x in alist[func])
			file.write(f"{line}\n");
		else:
			file.write(f"{alist[func]}\n");

	file.close()


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
