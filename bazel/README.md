## Command line args

* `--workspace_status_command=bazel/get_workspace_status` 
指定一个脚本在运行bazel之前会先这行这个脚本，这个脚本会输出会被放到`bazel-out/volatile-status.txt`文件中，可以被其他规则所使用。

