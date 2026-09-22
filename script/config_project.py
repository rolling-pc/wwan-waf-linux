import click
import json
import platform
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from waf_arch import get_windows_profile

os_type = platform.system()


if os_type == 'Linux':
    print("Running on Linux...")
elif os_type == 'Windows':
    print("Running on Windows...")

OPTIONS = {
    'release_type': {
        'choices': ["service(WSF)", "tools(WTF)", "cli", "all(Not Applicable in Windows)","Test"],
        'help': "Choose the type of release you want to configure.",
        'default': 'service(WSF)'
    },
    'oem_type': {
        'choices': ['hp', 'lenovo', 'dell', "chrome", "back"],
        'help': "Choose the OEM type you are working with.",
        'default': 'hp'
    },
    'project_name': {
        'choices': ['Qualcomm', 'MediaTek', 'Broad', "back"],
        'help': "Choose the name of the project you want to configure.You can select multiple options separated by commas (e.g., 1,2).",
        'default': 'Qualcomm',
        'multi_select': True
    },
    'switch_on_plugin': {
        'choices': ["none", "ma", "simmanager", "both", "back"],
        'help': "Choose the close function type. You can select multiple options separated by commas (e.g., 1,2).",
        'default': 'none',
        'multi_select': True
    },
    'package_type': {
        'choices': ['exe','deb', 'rpm', 'run', "back"],
        'help': "Choose the type of installation package.",
        'default': 'exe'
    },
    'target_arch': {
        'choices': ['x64', 'arm64', "back"],
        'help': "Choose the target CPU architecture (Windows only).",
        'default': 'x64'
    }
}

def prompt_choice(option_name, choices, help_text, default, multi_select=False):
    click.echo(click.style(f'{option_name.replace("_", " ").title()}: {help_text}', fg='cyan'))
    for i, choice in enumerate(choices, 1):
        click.echo(click.style(f'{i}: {choice}', fg='green'))

    if multi_select:
        prompt_msg = f'Please choose {option_name.lower()} (default: {default})'
        choice_str = click.prompt(prompt_msg, default=str(choices.index(default) + 1))
        try:
            choice_nums = [int(num.strip()) for num in choice_str.split(',')]
            selected_choices = [choices[num - 1] for num in choice_nums if 1 <= num <= len(choices)]
            if "back" in selected_choices:
                return "back"
            if not selected_choices:
                raise ValueError
            return selected_choices
        except ValueError:
            click.echo(click.style('Invalid choice. Please try again.', fg='red'))
            return prompt_choice(option_name, choices, help_text, default, multi_select)
    else:
        choice_num = click.prompt(
            f'Please choose {option_name.lower()} (default: {default})',
            type=int,
            default=choices.index(default) + 1
        )
        if option_name.lower() != "release type":
            if choice_num == len(choices):  # "back" option
                return "back"
        if 1 <= choice_num <= len(choices):
            return choices[choice_num - 1]
        else:
            click.echo(click.style('Invalid choice. Please try again.', fg='red'))
            return prompt_choice(option_name, choices, help_text, default)

def generate_cmake_presets(configurations):

    if os_type == 'Linux':
        compile_c = "/usr/bin/gcc"
        compile_cxx = "/usr/bin/g++"
    elif os_type == 'Windows':
        compile_c = "cl.exe"
        compile_cxx = "cl.exe"

    if configurations['release_type'] == 'Test':
        name = "Test"
        presets = {
            "version": 3,
            "cmakeMinimumRequired": {
                "major": 3,
                "minor": 19,
                "patch": 0
            },
            "configurePresets": [
                {
                    "name": name,
                    "description": f"Configure for {name} with function",
                    "binaryDir": "${sourceDir}/build",
                    "generator": "Ninja",
                    "environment": {
                        "CC": compile_c,
                        "CXX":compile_cxx
                    },
                    "cacheVariables": {
                        "CURRENT_PROJECT": configurations.get('project_name', 'none'),
                        "CURRENT_OEM": configurations.get('oem_type', 'none'),
                        "CURRENT_PACKAGE_TYPE": configurations.get('package_type', 'none'),
                        "PROJECT_UNIT_TEST": True,
                        "INSTALL_SERVICE_ENABLE": False,
                        "INSTALL_TOOLS_ENABLE": False,
                        "INSTALL_CLI_ENABLE": False
                    }
                }
            ],
            "buildPresets": [
                {
                    "name": f"build-{name}",
                    "configurePreset": name
                }
            ]
        }
    else:
        presets = {
            "version": 3,
            "cmakeMinimumRequired": {
                "major": 3,
                "minor": 19,
                "patch": 0
            },
            "configurePresets": [],
            "buildPresets": []
        }

        # 处理非 Test 的情况
        if configurations['release_type'] == "service(WSF)":
            configurations['release_type'] = "service"
            release_types = ['service']
        elif configurations['release_type'] == "tools(WTF)":
            configurations['release_type'] = "all(WAF)"
            if os_type == 'Linux':
                # Linux tool deb: service + tools only, skip cli
                release_types = ['service', 'tools']
            else:
                # Windows: keep original flow (cli + service + tools)
                release_types = ['cli', 'service', 'tools']
        elif "all" in configurations['release_type']:
            release_types = ['cli', 'service', 'tools']
        else:
            release_types = [configurations['release_type']]
        project_value = ""
        if configurations['release_type'] in ['tools', 'cli']:
            pass
        else:
            if len(configurations['project_name']) != 1:
                project_value = configurations.get('project_name')
                configurations['project_name'] = "multiple"
            else:
                configurations['project_name'] =configurations['project_name'][0]
        # 保存 service preset 需要的 OEM/项目名（下面的 tools/cli 循环会临时清空它们）
        saved_oem = configurations.get('oem_type', '')
        saved_project = configurations.get('project_name', '')
        comp_data=''
        tool_service = False
        service_needed = 'service' in release_types
        tools_needed = 'tools' in release_types
        cli_needed = 'cli' in release_types
        
        # 确定是否需要编译 tools（只有当明确选择 tools 或只编译 tools 时才需要）
        if configurations['release_type'] == "tools(WTF)":
            tool_service = True
        elif "all" in configurations['release_type']:
            # all = cli + service + tools
            tool_service = True
        else:
            tool_service = False
        for release_type in release_types:
            if release_type in ['tools', 'cli']:
                # comp_data = configurations['project_name'];
                configurations['oem_type'] = ''
                configurations['project_name'] = ''
            else:
                # service：恢复 OEM/项目名（防止被前面的 cli 循环清空）
                configurations['oem_type'] = saved_oem
                configurations['project_name'] = saved_project
            
            # 根据当前 release_type 设置编译选项
            install_service = service_needed and release_type == 'service'
            if os_type == 'Linux':
                # Linux service preset 已通过 wsf 引入 lib，不能再同时开启 INSTALL_TOOLS_ENABLE
                install_tools = tools_needed and release_type == 'tools'
            else:
                install_tools = tools_needed and (release_type == 'tools' or release_type == 'service')
            install_cli = cli_needed and release_type == 'cli'

            name_parts = [
                configurations.get('oem_type', ''),
                configurations.get('project_name', ''),
                release_type,
                configurations.get('package_type', '')
            ]
            name = "-".join(part for part in name_parts if part)
            if len(configurations['project_name']) != 1 and configurations['project_name'] == "multiple":
                configurations['project_name'] = ','.join(project_value)
            pluginstr = configurations.get('switch_on_plugin', '')

            # not consider install on Windows yet
            if os_type == 'Linux':
                presets['configurePresets'].append({
                    "name": name,
                    "description": f"Configure for {name} with function",
                    "binaryDir": "${sourceDir}/build",
                    "generator": "Ninja",
                     "environment": {
                            "CC": compile_c,
                            "CXX":compile_cxx
                    },
                    "cacheVariables": {
                        "CURRENT_PROJECT": comp_data if comp_data else configurations.get('project_name', 'none'),
                        "CURRENT_OEM": configurations.get('oem_type', 'none'),
                        "CURRENT_PACKAGE_TYPE": configurations.get('package_type', 'none'),
                        "PROJECT_UNIT_TEST": False,
                        "PROJECT_PLUGIN_MA": 'ma' in configurations.get('switch_on_plugin', 'none'),
                        "PROJECT_PLUGIN_MA1": 'ma1' in configurations.get('switch_on_plugin', 'none'),
                        "INSTALL_SERVICE_ENABLE": install_service,
                        "INSTALL_TOOLS_ENABLE": install_tools,
                        "INSTALL_CLI_ENABLE": install_cli
                    }
                })
            else:    
                target_arch = configurations.get('target_arch', 'x64')
                win_profile = get_windows_profile(target_arch)
                presets['configurePresets'].append({
                    "name": name,
                    "description": f"Configure for {name} with function",
                    "binaryDir": "${sourceDir}/build",
                    "generator": "Ninja",
                     "environment": {
                            "CC": compile_c,
                            "CXX":compile_cxx
                    },
                    "cacheVariables": {
                        "CURRENT_PROJECT": configurations.get('project_name', 'none'),
                        "CURRENT_OEM": configurations.get('oem_type', 'none'),
                        "CURRENT_PACKAGE_TYPE": configurations.get('package_type', 'none'),
                        "PROJECT_UNIT_TEST": False,
                        "PROJECT_PLUGIN_MA": 'ma' in pluginstr or 'both' in pluginstr,
                        "PROJECT_PLUGIN_SIMMANAGER": 'simmanager' in pluginstr or 'both' in pluginstr,
                        "INSTALL_SERVICE_ENABLE": install_service,
                        "INSTALL_TOOLS_ENABLE": install_tools,
                        "INSTALL_CLI_ENABLE": install_cli,
                        "CMAKE_BUILD_TYPE": "Release",
                        "WAF_TARGET_ARCH": target_arch,
                        "CMAKE_PREFIX_PATH": win_profile["qt_prefix"],
                        "ZLIB_LIBRARY": win_profile["zlib_library"],
                        "ZLIB_INCLUDE_DIR": win_profile["zlib_include"],
                        "BZIP2_SOURCE_DIR": "${sourceDir}/third_party/common/quazip-master/bzip2_1.0.8-src",
                    }
                })

            presets['buildPresets'].append({
                "name": f"build-{name}",
                "configurePreset": name
            })

    with open('CMakePresets.json', 'w') as f:
        json.dump(presets, f, indent=4)
    click.echo('CMakePresets.json has been generated.')




@click.command()
@click.argument('release_type', required=False)
#@click.argument('oem_type', required=False)
@click.argument('project_name', required=False)
@click.argument('package_type', required=False)
@click.argument('switch_on_plugin', required=False)
@click.argument('target_arch', required=False)
@click.option('--help', '-h', is_flag=True, help='Show this message and exit.')
#def configure(release_type, oem_type, project_name, package_type, switch_on_plugin, help):
def configure(release_type, project_name,package_type, switch_on_plugin, target_arch, help):
    if help:
        click.echo("This command will help you configure a project with the following options:")
        click.echo("Position arguments:")
        #click.echo("  oem_type        Choose the OEM type you are working with.")
        #click.echo("  project_name    Choose the name of the project you want to configure.You can select multiple options separated by commas (e.g., 1,3).")
        click.echo("  package_type    Choose the type of installation package.")
        click.echo("  switch_on_plugin  Choose the switch on plugin. You can select multiple options separated by commas (e.g., 1,3).")
        click.echo("  release_type    Choose the type of release you want to configure.")
        return

    selections = {}
    stack = []

    def select_release_type():
        stack.append('release_type')
        while True:
            selections['release_type'] = release_type or prompt_choice(
                'Release type',
                OPTIONS['release_type']['choices'],
                OPTIONS['release_type']['help'],
                OPTIONS['release_type']['default']
            )
            if selections['release_type'] == "back":
                if len(stack) > 1:
                    stack.pop()
                    return False
            else:
                break
        return True

    def select_project_name():
        stack.append('project_name')
        while True:
            # selections['project_name'] = project_name or prompt_choice(
            selections['project_name'] = project_name.split(',') if project_name else prompt_choice(
                'Project name',
                OPTIONS['project_name']['choices'],
                OPTIONS['project_name']['help'],
                OPTIONS['project_name']['default'],
                multi_select=True
            )
            if selections['project_name'] == "back":
                if len(stack) > 1:
                    stack.pop()
                    return False
            else:
                break
        return True


    def select_oem_type():
        stack.append('oem_type')
        while True:
            selections['oem_type'] = oem_type or prompt_choice(
                'OEM type',
                OPTIONS['oem_type']['choices'],
                OPTIONS['oem_type']['help'],
                OPTIONS['oem_type']['default']
            )
            if selections['oem_type'] == "back":
                if len(stack) > 1:
                    stack.pop()
                    return False
            else:
                break
        return True

    def select_switch_on_plugin():
        stack.append('switch_on_plugin')
        while True:
            selections['switch_on_plugin'] = switch_on_plugin.split(',') if switch_on_plugin else prompt_choice(
                'Switch on function',
                OPTIONS['switch_on_plugin']['choices'],
                OPTIONS['switch_on_plugin']['help'],
                OPTIONS['switch_on_plugin']['default'],
                multi_select=True
            )
            if selections['switch_on_plugin'] == "back":
                if len(stack) > 1:
                    stack.pop()
                    return False
            else:
                break
        return True

    def select_target_arch():
        if os_type != 'Windows':
            selections['target_arch'] = 'x64'
            return True
        stack.append('target_arch')
        while True:
            selections['target_arch'] = target_arch or prompt_choice(
                'Target arch',
                OPTIONS['target_arch']['choices'],
                OPTIONS['target_arch']['help'],
                OPTIONS['target_arch']['default']
            )
            if selections['target_arch'] == "back":
                if len(stack) > 1:
                    stack.pop()
                    return False
            else:
                break
        return True

    def select_package_type():
        stack.append('package_type')
        while True:
            selections['package_type'] = package_type or prompt_choice(
                'Package type',
                OPTIONS['package_type']['choices'],
                OPTIONS['package_type']['help'],
                OPTIONS['package_type']['default']
            )
            if selections['package_type'] == "back":
                if len(stack) > 1:
                    stack.pop()
                    return False
            else:
                break
        return True
    value = ''
    while True:
        if value != '' and value != "release_type":
            pass
        else:
            if not select_release_type():
                selections['release_type'] = ''
                continue
            value = ''

        if selections['release_type'] in ['service(WSF)', 'tools(WTF)', 'all(WAF)', 'all(Not Applicable in Windows)']:
            if selections['release_type'] in ['service(WSF)', 'all(WAF)', 'all(Not Applicable in Windows)']:
                if value != '' and value != "oem_type":
                    pass
                else:
                    selections['oem_type'] = 'generic'
                    value = ''
                '''
                else:
                    if not select_oem_type():
                        if (len(stack) != 0):
                            value = stack.pop()
                            if value == "release_type":
                                selections['oem_type'] = ''
                                continue
                    value = ''
                    # continue
                '''

            if value != '' and value != "project_name":
                pass
            else:

                if not select_project_name():
                    if (len(stack) != 0):
                        value = stack.pop()
                        if value == "oem_type":
                            selections['project_name'] = ''
                            continue

                value = ''

                if value != '' and value != "switch_on_plugin":
                    pass
                else:
                    if not select_switch_on_plugin():
                        if (len(stack) != 0):
                            value = stack.pop()
                            if value == "project_name":
                                selections['switch_on_plugin'] = ''
                                continue
                    value = ''
                    if os_type == 'Linux' or os_type == 'Windows':
                        if not select_target_arch():
                            if (len(stack) != 0):
                                value = stack.pop()
                                if value == "switch_on_plugin":
                                    selections['target_arch'] = ''
                                    continue
                        value = ''
                        if not select_package_type():
                            if (len(stack) != 0):
                                value = stack.pop()
                                if value == "target_arch":
                                    selections['package_type'] = ''
                                    continue
                            value = ''
        else :
            if selections['release_type'] in ['tools(WTF)', 'all(WAF)']:

                if value != '' and value != "package_type":
                    pass
                else:
                    if not select_package_type():
                        if (len(stack) != 0):
                            value = stack.pop()
                            if value == "company_name":
                                selections['package_type'] = ''
                                continue
                        value = ''
        break

    generate_cmake_presets(selections)

if __name__ == '__main__':
    configure()