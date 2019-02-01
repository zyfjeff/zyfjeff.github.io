## 开启ftrace

```
sudo mount -t debugfs nodev /sys/kernel/debug/
```

## trace cmd

1. record - record a trace into a trace.dat file
2. start - start tracing without recording into a file
3. extract - extract a trace from the kernel
4. stop - stop the kernel from recording trace data
5. show - show the contents of the kernel tracing buffer
6. reset - disable all kernel tracing and clear the trace buffers
7. report - read out the trace stored in a trace.dat file
8. hist - show a historgram of the trace.dat information
9. split - parse a trace.dat file into smaller file(s)
10. options - list the plugin options available for trace-cmd report
11. listen - listen on a network socket for trace clients
12. list - list the available events, plugins or options
13. restore - restore a crashed record
14. snapshot - take snapshot of running trace
15. stack - output, enable or disable kernel stack tracing
16. check-events - parse trace event formats

git://git.kernel.org/pub/scm/linux/kernel/git/rostedt/trace-cmd.git


## Function tracing

```
// function tracing
# cd /sys/kernel/debug/tracing
# echo function > current_tracer
# cat trace

// dittio
# trace-cmd start -p function
# trace-cmd show

// stop tarcing
# echo nop > current_tracer # cat trace
# trace-cmd start -p nop

// function graph tracing
# echo function_graph > current_tracer
# cat trace

# trace-cmd start -p function_graph -g SyS_read
# trace-cmd show

// function tracing filter
# set_ftrace_filter   // 只对相关的函进行tracing
# set_ftrace_notrace  // 不对列在其中的函数进行tracing，会覆盖set_ftrace_filter中的内容
# available_filter_functions  // 列出可以加入到上述文件中的函数列表
# set_graph_function // 对指定函数，生成function graph
```

## Function triggers


`<function-name>:<trigger>:<count>`
trigger:
  1. traceoff
  2. traceon
  3. stacktrace
  4. dump
  5. cpudump
  6. enable_event/disable_event