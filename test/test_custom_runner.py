import os
import subprocess
import shutil
import click
from platformio.test.result import TestCase, TestStatus
from platformio.test.runners.base import TestRunnerBase

class CustomTestRunner(TestRunnerBase):
    def stage_building(self):
        pass

    def stage_testing(self):
        project_dir = self.project_config.get("platformio", "core_dir")
        cwd = os.getcwd()
        
        # Look for vcvars64 or cl.exe or g++
        tests = [
            ("test_sensor_math", ["src/sensor_reader.cpp", "test/test_sensor_math.cpp"], []),
            ("test_battery_math", ["src/battery_monitor.cpp", "test/test_battery_math.cpp"], []),
            ("test_ble_profile", ["test/test_ble_profile.cpp"], ["ERGOBIKE_FTMS_ONLY=1"]),
            ("test_ftms_control_point", ["test/test_ftms_control_point.cpp"], []),
            ("test_ftms_indoor_bike_data", ["test/test_ftms_indoor_bike_data.cpp"], []),
            ("test_csc_control_point", ["test/test_csc_control_point.cpp"], []),
            ("test_config_defaults", ["src/storage_manager.cpp", "test/test_config_defaults.cpp"], []),
        ]
        
        # Check if cl is available or vswhere
        vcvars = r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
        use_msvc = os.path.exists(vcvars)
        
        for name, sources, defines in tests:
            exe_name = f"{name}.exe"
            exe_path = os.path.join(cwd, exe_name)
            
            status = TestStatus.PASSED
            message = None
            
            try:
                if use_msvc:
                    src_args = " ".join(sources)
                    define_args = " ".join(f"/D{define}" for define in defines)
                    cmd = f'"{vcvars}" >nul && cl /nologo /EHsc /std:c++17 /DNATIVE_TEST {define_args} /Iinclude {src_args} /Fe:{exe_name}'
                    build_proc = subprocess.run(cmd, shell=True, capture_output=True, text=True)
                    if build_proc.returncode != 0:
                        click.secho(f"Build failed for {name}:\n{build_proc.stderr or build_proc.stdout}", fg="red")
                        status = TestStatus.FAILED
                        message = "Build failed"
                        self.test_suite.add_case(TestCase(name=name, status=status, message=message))
                        continue
                else:
                    define_args = [f"-D{define}" for define in defines]
                    cmd = ["g++", "-std=c++17", "-DNATIVE_TEST"] + define_args + ["-Iinclude"] + sources + ["-o", exe_name]
                    build_proc = subprocess.run(cmd, capture_output=True, text=True)
                    if build_proc.returncode != 0:
                        click.secho(f"Build failed for {name}:\n{build_proc.stderr}", fg="red")
                        status = TestStatus.FAILED
                        message = "Build failed"
                        self.test_suite.add_case(TestCase(name=name, status=status, message=message))
                        continue

                # Run test
                run_proc = subprocess.run([exe_path], capture_output=True, text=True)
                click.echo(run_proc.stdout)
                if run_proc.returncode == 0:
                    status = TestStatus.PASSED
                else:
                    click.secho(f"Test failed with returncode {run_proc.returncode}:\n{run_proc.stderr}", fg="red")
                    status = TestStatus.FAILED
                    message = run_proc.stderr

            except Exception as e:
                status = TestStatus.ERRORED
                message = str(e)
            finally:
                if os.path.exists(exe_path):
                    try:
                        os.remove(exe_path)
                    except OSError:
                        pass
                for obj in [f"{os.path.splitext(os.path.basename(s))[0]}.obj" for s in sources]:
                    if os.path.exists(obj):
                        try:
                            os.remove(obj)
                        except OSError:
                            pass

            self.test_suite.add_case(TestCase(name=name, status=status, message=message))
