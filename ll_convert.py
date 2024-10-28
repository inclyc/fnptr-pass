from subprocess import run
import os
import os.path


def convert(input_file, output_file):
    run(["clang", input_file, "-O0", "-emit-llvm", "-S", "-o", output_file], check=True)


def convert_dir(input_dir, output_dir):
    os.makedirs(output_dir, exist_ok=True)

    with os.scandir(input_dir) as it:
        list(
            map(
                lambda x: convert(
                    x.path,
                    os.path.join(output_dir, x.name.replace(".c", ".ll")),
                ),
                it,
            )
        )


convert_dir("assign2-tests", "assign2-ll")
