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

# Change the current working directory
os.chdir(build_dir)

# Run cmake command
run_command("cmake ..")

# Run msbuild command
run_command("msbuild Knowboy.sln /p:Configuration=Release")

run_command(r'move .\bin\Release\knowboy_libretro.dll C:\RetroArch-Win64\cores\knowboy_libretro.dll')
# Remove-Item * -Recurse -Force