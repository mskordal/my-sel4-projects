import csv
import statistics
import os
import sys
import pprint
import numpy as np
import matplotlib.pyplot as plt
from scipy.stats import norm

class Prof_func:
	def __init__(self, name, lvl):
		self.name = name
		self.lvl = lvl
		self.cycles_list = []
		self.event_lists = [[], [], [], [], [], []]

	# def __str__(self):
		# return "%: %" (self.name, self.lvl)

	def __repr__(self):
		return (
			"% s: % s cycle size: % s" %
			(self.name, self.lvl, len(self.cycles_list))
		)

CWD = os.path.dirname(os.path.realpath(__file__));
FUNCTIONS_FILE_PATH = CWD + "/../accProfPass/functions.txt"

event_names = []
events_num = 0
csv_files = []
funcs = {}

def main():
	print("-------------------------")
	print("Gaussian generator script")
	print("-------------------------")
	path = sys.argv[1]
	if path[-1] != '/':
		path += "/"
	action = sys.argv[2]
	if action != "plot" and action != "shift":
		sys.exit("2nd arg should be \"plot\" to plot or \"shift\" to generate"
		   " how many bits to shift")

	init_globals(path)
	print("events: ", event_names)
	print("functions:")
	for key, val in funcs.items():
		print("\t", key, val)

	prof_funcs = read_csv_files(path)
	print(prof_funcs)

	prof_func = prof_funcs[0]

	if action == "plot":
		plot_gaussian(prof_func.name, prof_func.lvl, prof_func.event_lists[0])
	else:
		generate_shifts(prof_funcs)


def init_globals(path: str):
	global event_names, funcs, csv_files, events_num

	files = os.listdir(path)
	csv_files = [file for file in files if file.endswith('.csv')]
	with open(path + csv_files[0], 'r') as csv_file:
		csv_reader = csv.reader(csv_file, delimiter=',')
		event_names = next(csv_reader)[3:]
	events_num = len(event_names)
	with open(FUNCTIONS_FILE_PATH, 'r') as functions_file:
		for line in functions_file:
			val, key = line.split()
			funcs[int(key)] = val


def read_csv_files(path: str):
	global event_names, funcs, csv_files, events_num

	prof_funcs = []

	with open(path + csv_files[0], 'r') as csv_file:
		csv_reader = csv.reader(csv_file, delimiter=',')
		_ = next(csv_reader)
		for row in csv_reader:
			func_name = funcs[int(row[0])]
			func_lvl = int(row[1])
			curr_prof_func = find_prof_func(prof_funcs, func_name, func_lvl)
			if not curr_prof_func:
				curr_prof_func = Prof_func(func_name, func_lvl)
				prof_funcs.append(curr_prof_func)
			curr_prof_func.cycles_list.append(int(row[2]))
			for i in range(events_num):
				curr_prof_func.event_lists[i].append(int(row[3+i]))

	return prof_funcs


def plot_gaussian(func_name: str, func_lvl: int, event_data: list[int]):
	data = np.array(event_data)
	mu, sigma = np.mean(data), np.std(data)
	x = np.linspace(min(data) - 3*sigma, max(data) + 3*sigma, 1000)
	y = norm.pdf(x, mu, sigma)

	plt.plot(x, y, label=f'Gaussian Fit ($\mu$={mu:.2f}, $\sigma$={sigma:.2f})')
	plt.hist(data, bins=20, density=True, alpha=0.6, color='g', label='Data Histogram')
	plt.xlabel('Value')
	plt.ylabel('Probability Density')
	plt.title('Gaussian Distribution Fit')
	plt.legend()
	plt.grid(True)
	plt.show()


def generate_shifts(prof_funcs: list[Prof_func]):
	global events_num

	for i in range(events_num):
		data = np.array(event_data)

def find_prof_func(prof_funcs: list[Prof_func], name: str, lvl: int) \
	-> Prof_func:
	for prof_func in prof_funcs:
		if prof_func.name == name and prof_func.lvl == lvl:
			return prof_func
	return None


if __name__ == "__main__":
	 main()
