import os
import subprocess

def run_command(command):
    """Run a system command and handle exceptions."""
    try:
        subprocess.run(command, check=True, shell=True)
    except subprocess.CalledProcessError as e:
        print(f"Error occurred while executing: {command}")
        print(f"Error details: {e}")

# Directory to be created and used
build_dir = "build"

# Check if the directory exists, create it if it doesn't
if not os.path.exists(build_dir):
    os.makedirs(build_dir)

# Run cmake command with the build directory as an argument
run_command(f"cmake -B{build_dir} -H.")

# Run msbuild command
run_command(f"msbuild {build_dir}/Knowboy.sln /p:Configuration=Release")