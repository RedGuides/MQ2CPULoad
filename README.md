# MQ2CpuLoad

This plugin acts as a CPU load balancer for EQ.

Using this plugin you will be able to dynamically adjust which instances
are running on which cores or allow the plugin to automatically make sure
instance in the foreground is on its own cpu.

## Commands

| Command               | Action                                          |
| :-------------------- | :---------------------------------------------- |
| `/cpu`                | shows basic status                              |
| `/cpu help`           | shows help info                                 |
| `/cpu auto`           | turns on auto balancing                         |
| `/cpu manual`         | turns off auto balancing                        |
| `/cpu set <core>`     | manually moves current instance to another core |
| `/cpu high`           | flags current instance as high priority         |
| `/cpu low`            | flags current instance as low priority          |
| `/cpu report <level>` | how much debugging info do you want?            |

## Author

Original code by Dewey2461
