## Analyzing a stopped container

```
docker diff <container_id>
docker cp <container_id>:/var/log/nginx/error.log .
docker commit <container_id> debugimage
docker run -ti --entrypoint sh debugimage
```

## When to use JSON syntax and string syntax

1. String syntax
  * is easier to write
  * interpolates environment variables and other shell expressions
  * creates an extra process (/bin/sh -c ...) to parse the string
  * *requires /bin/sh to exist in the container*

2. JSON syntax
  * is harder to write (and read!)
  * passes all arguments without extra processing
  * doesn't create an extra process
  * *doesn't require /bin/sh to exist in the container*

## multi-stage builds

```
FROM golang AS builder
RUN ...
FROM alpine
COPY --from=builder /go/bin/mylittlebinary /usr/local/bin/
```

## Dockerfile ADD command

ADD works almost like COPY, but has a few extra features.ADD can get remote files

```
ADD http://www.example.com/webapp.jar /opt/
```

This would download the webapp.jar file and place it in the /opt directory. ADD will automatically unpack zip files and tar archives

```
ADD ./assets.zip /var/www/htdocs/assets/
```
This would unpack assets.zip into /var/www/htdocs/assets.However, ADD will not automatically unpack remote archives.

## OCI
OCI是业界合作努力来定义关于容器格式和运行时的开放容器规范，包含了镜像规范和运行时规范,
