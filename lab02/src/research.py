import subprocess
import time
import os

def generate_input_file(filename, num_arrays, elements_per_array):
    with open(filename, 'w') as f:
        f.write(f"{num_arrays} {elements_per_array}\n")
        for _ in range(num_arrays):
            array = " ".join(str(x) for x in range(1, elements_per_array + 1))
            f.write(array + "\n")

def run_experiment(executable, input_file, num_threads, output_file):
    start_time = time.time()
    result = subprocess.run([executable, input_file, str(num_threads), output_file], capture_output=True, text=True)
    end_time = time.time()

    if result.returncode != 0:
        print(f"Error: {result.stderr}")
        return None

    time_elapsed = end_time - start_time
    return time_elapsed

def main():
    executable = "./main"  
    thread_counts = [1, 2, 6, 10]  
    configurations = [
        (5, 5),  
        (10, 100), 
        (100, 1000),
    ]
    input_folder = "input_data"
    output_folder = "output_data"
    os.makedirs(input_folder, exist_ok=True)
    os.makedirs(output_folder, exist_ok=True)

    input_files = []
    for num_arrays, elements_per_array in configurations:
        filename = os.path.join(input_folder, f"input_{num_arrays}_{elements_per_array}.txt")
        generate_input_file(filename, num_arrays, elements_per_array)
        input_files.append(filename)

    results = {}
    for input_file in input_files:
        file_results = {}
        base_name = os.path.splitext(os.path.basename(input_file))[0]
        for threads in thread_counts:
            output_file = os.path.join(output_folder, f"{base_name}_output_{threads}.txt")
            time_elapsed = run_experiment(executable, input_file, threads, output_file)
            if time_elapsed is not None:
                file_results[threads] = time_elapsed
            else:
                file_results[threads] = float('inf')

        results[input_file] = file_results

    for input_file, timings in results.items():
        config = os.path.splitext(os.path.basename(input_file))[0].split('_')[1:]
        num_arrays, elements_per_array = map(int, config)
        t1 = timings[1] 

        print(f"\nResults for {num_arrays} arrays of size {elements_per_array}:")
        for threads, time_elapsed in sorted(timings.items()):
            if time_elapsed == float('inf'):
                continue
            speedup = t1 / time_elapsed
            efficiency = speedup / threads
            print(f"Threads: {threads}, Time: {time_elapsed:.6f} sec, Speedup: {speedup:.2f}, Efficiency: {efficiency:.2f}")

if __name__ == "__main__":
    main()
