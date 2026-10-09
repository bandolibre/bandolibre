Website at http://bandolibre.github.io

Main project (firmware, hardware, docs): https://github.com/bandolibre/bandolibre-main

## Serve locally with Jekyll:

First you have to update the dependencies with the `bundle` tool once.
That tool fails when used with IPV6, in that case run:

```
sudo sysctl -w net.ipv6.conf.all.disable_ipv6=1
sudo sysctl -w net.ipv6.conf.default.disable_ipv6=1
bundle
sudo sysctl -w net.ipv6.conf.all.disable_ipv6=0
sudo sysctl -w net.ipv6.conf.default.disable_ipv6=0
```

An then:

```
bundle exec jekyll serve
```
