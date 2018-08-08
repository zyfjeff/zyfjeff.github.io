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
