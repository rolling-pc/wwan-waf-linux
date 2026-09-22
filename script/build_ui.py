import os
import subprocess
import sys
import shutil
from pathlib import Path

def run_command(command):
    try:
        subprocess.run(command, check=True, shell=True)
    except subprocess.CalledProcessError as e:
        print(f"Command '{command}' failed with error: {e}")
        exit(1)
def copy_singleFile(src_folder,dest_folder,filename):
    source_file = os.path.join(src_folder, filename)
    destination_file = os.path.join(dest_folder, filename)
    shutil.copy(source_file, destination_file)
def main():
    print("build ui start")

    if sys.platform.startswith("win"):
        print("Windows")
    elif sys.platform.startswith("linux"):
        print("Linux")
        current_dir_path = os.getcwd()
        destination_folder = current_dir_path + "/../../../../src/wtf/tool_kits_ui/public"             
        source_folder = current_dir_path + "/../../../../script/linux"        
        copy_singleFile(source_folder, destination_folder, 'WAF_User_Guide.pdf')

    # Change to the target directory
    ui_path = Path('../../../../src/wtf/tool_kits_ui').resolve()
    os.chdir(ui_path)

    # Check if "node_modules" directory exists, if not, run "npm i"
    if not Path("node_modules").exists():
        run_command("npm config set registry http://mirrors.cloud.tencent.com/npm/")
        run_command("npm install")

    # Run the build command
    run_command("npm run build")

    # Create the destination directory if it doesn't exist
    build_ui_path = Path('../../../build/UI').resolve()
    build_ui_path.mkdir(parents=True, exist_ok=True)

    # Copy the build files to the destination directory
    src_build_path = ui_path / 'build'
    for item in src_build_path.iterdir():
        dest = build_ui_path / item.name
        if item.is_dir():
            shutil.copytree(item, dest, dirs_exist_ok=True)
        else:
            shutil.copy2(item, dest)

    # Remove the specified map file if it exists
    map_file = build_ui_path / 'static/js/main.b10eecc6.js.map'
    if map_file.exists():
        map_file.unlink()

    print("build and copy ready!")

if __name__ == "__main__":
    main()
