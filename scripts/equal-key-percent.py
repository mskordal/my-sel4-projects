import sys
import os
import matplotlib.pyplot as plt
import numpy as np

class Res_file:
	def __init__(self, var_idx, result_arr, warmup):
		self.var_idx = var_idx
		self.result_arr = result_arr
		self.result_arr_warm = result_arr[warmup:]
		valids_num = result_arr.count("4")
		invalids_num = len(result_arr) - valids_num
		self.percent = (valids_num * 100)/(valids_num + invalids_num)
		valids_num_warm = self.result_arr_warm.count("4")
		invalids_num_warm = len(self.result_arr_warm) - valids_num_warm
		self.percent_warm = (valids_num_warm * 100)/(valids_num_warm + invalids_num_warm)

	def __repr__(self):
		return f"Var Idx: {self.var_idx}\n\t{self.percent:.2f}%\n\t{self.percent_warm:.2f}%\n"

CWD = os.path.dirname(os.path.realpath(__file__));
EVENTS_FILE_PATH = CWD + "/../accProfPass/events.txt"
VARIANCES_FILE_PATH = CWD + "/../accProfPass/variances.txt"

events = []
variances = []
res_files = {}
warmup = 0

def main():
	global res_files, warmup, events, variances
	results_dir = os.path.abspath(sys.argv[1])
	warmup = int(sys.argv[2])

	# Read current events
	with open(EVENTS_FILE_PATH, 'r') as events_file:
		for line in events_file:
			clean_line = line.replace("\n","")
			events.append(clean_line)
	print(events)
	
	# Read current events
	with open(VARIANCES_FILE_PATH, 'r') as variances_file:
		for line in variances_file:
			clean_line = line.replace("\n","")
			split_list = clean_line.split(" ")
			variances.append(split_list)
	print(variances)

	# Organise attestation results to classes
	for file in os.listdir(results_dir):
		# Check if this var idx is already registered
		var_idx = int(file[3])
		res_file = res_files.get(var_idx)
		if res_file:
			continue
		# If not scan file to create object
		result_arr = ""
		with open(results_dir + "/" + file, 'r') as file_reader:
			for line in file_reader:
				strip_line = line.strip()
				if not strip_line:
					continue
				split_line = strip_line.split()[1]
				clean_line = split_line.replace("0","")
				clean_line_reverse = clean_line[::-1]
				final_line = clean_line_reverse[0:len(clean_line_reverse)-1]
				result_arr += final_line
		res_file = Res_file(var_idx, result_arr, warmup)
		res_files[var_idx] = res_file

	plot_res()

def plot_res():
	global res_files, warmup, events, variances
	# Dummy data
	x_coords = generate_x_coords()
	y_vals = generate_percents()
	y_vals_warm = generate_percents_warm()
	x = np.arange(len(x_coords))
	offset = 0.4
	# Create the bar chart
	plt.figure(figsize=(18, 8))
	plt.bar(x - offset/2, width=offset, height=y_vals, color='black',
		label="Cold Execution", alpha=0.4)
	plt.bar(x + offset/2, width=offset, height=y_vals_warm, color='black',
		label="Warm Execution", alpha=1)
	plt.xlabel("Additional bits discarded per event", fontsize=18)
	plt.ylabel("Percentage of correct function states", fontsize=18)
	plt.title("Function State Corretness Assession, Prime n Probe, STREAM (32 runs), 1 million array size", fontsize=20)
	plt.legend(fontsize=18)
	plt.xticks(ticks=x, labels=x_coords, fontsize=10)
	plt.ylim(0, 100)  # Keep within 0-100% range
	plt.grid(axis='y', linestyle='--', alpha=1)
	# plt.savefig("plot.svg")
	plt.show()
	


def generate_x_coords() -> list[str]:
	x_coords = []
	for i in range(len(res_files)):
		coord = ""
		for event_idx in range(len(events)):
			coord += events[event_idx] + ": " + variances[i][event_idx] + "\n"
		x_coords.append(coord)
	print(x_coords)
	return x_coords


def generate_percents() -> list[float]:
	y_vals = []
	for i in range(len(res_files)):
		y_vals.append(res_files[i].percent)
	return y_vals


def generate_percents_warm() -> list[float]:
	y_vals = []
	for i in range(len(res_files)):
		y_vals.append(res_files[i].percent_warm)
	return y_vals

if __name__ == "__main__":
	main()
