from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1]
subprocess.run([sys.executable,str(root/'shared-kit/tests/run.py')],check=True)
subprocess.run(['node','tests/consumer.test.js'],cwd=root,check=True)

subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-DLAB_HOST_RENDER',
 '-Ishared-kit/tests/health-stubs','tests/name.test.c','src/c/quest_settings.c','-o','build/name-tests'],cwd=root,check=True)
subprocess.run([str(root/'build/name-tests')],check=True)
