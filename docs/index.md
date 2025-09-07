---
tags:
  - plugin
resource_link: "https://www.redguides.com/community/resources/mq2cpuload.113/"
support_link: "https://www.redguides.com/community/threads/mq2cpuload.45707/"
repository: "https://github.com/RedGuides/MQ2CPULoad"
config: "MQ2CpuLoad.ini"
authors: "dewey2461"
tagline: "CPU load balancer for multiple EQ sessions. Will assign top (keyboard-focus) instance itss own core."
---

# MQ2CPULoad

<!--desc-start-->
This plugin acts as a CPU load balancer for EQ.

Using this plugin you will be able to dynamically adjust which instances
are running on which cores or allow the plugin to automatically make sure
instance in the foreground is on its own cpu.
<!--desc-end-->

## Commands

<a href="cmd-cpu/">
{% 
  include-markdown "projects/mq2cpuload/cmd-cpu.md" 
  start="<!--cmd-syntax-start-->" 
  end="<!--cmd-syntax-end-->" 
%}
</a>
:    {% include-markdown "projects/mq2cpuload/cmd-cpu.md" 
        start="<!--cmd-desc-start-->" 
        end="<!--cmd-desc-end-->" 
        trailing-newlines=false 
     %} {{ readMore('projects/mq2cpuload/cmd-cpu.md') }}

## Settings

Sample config:

```ini
[Priority]
Gandalf=0
Gimli=1
Bilbo=0
[Settings]
Reporting=0
cpuAutoBalance=1
```

- **Priority:** High or low CPU priority for character. 1 is high, 0 low.
- **Reporpting:** Reporting level, how much reporting do you want?
- **cpuAutoBalance:** automatic is 1, manual is 0
