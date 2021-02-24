* `printf("%-5s", xxx)`  左对齐，宽度是5，不足的用空格补全
* `echo -n` 不输出换行符
* `echo -e` 转义字符生效
* 通过`/proc/$PID/environ` 查看其他进程的环境变量，是以'\0'来分割多个变量
* `${#var}` 获取变量的长度
* `echo $SHELL` 或 `echo $0` 获取当前的shell类型
* PS1环境变量可以用于修改当前的命令行提示符
* 路径添加函数

```
prepend() { [ -d "$2" ] && eval $1=\"$2':'\$$1\" && export $1; }

example:
prepend PATH /opt/myapp/bin
```
* `${parameter:+expression}` 如果parameter有值不为空，则使用expression
*  ``算数操作

1. `let`

```
let result=no1+no2
let no1++
let no2++
let no+=6
let no-=6
```

2. `[]`

```
result=$[ no1 + no2 ]
result=$[ $no1 + 5 ]
```

3. `$(())`

```
result=$(( no1 + 50 ))
```

4. `expr`

```
result=`expr 3 + 4`
result=$(expr $no1 + 5)
```

5. `bc`

```
echo "4 * 0.56" | bc
no=54
result=`echo "$no * 1.5" | bc`
echo $result
echo "scale=2;22/7" | bc
np=100
echo "obase=2;$no" | bc
echo "obase=10;ibase=2;$no" | bc
echo "sqrt(100)" | bc
```

* 重定向到文本块

```
cat << EOF > log.txt
EOF
```

* 自定义描述符

```
exec 3<input.txt
exec 4>output.txt
exec 5>>output_append.txt
```
