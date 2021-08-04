# Shremote!

Execute commands remotely over SSH!

Usage:
```shell
usage: shremote_new.py [-h] [--parse-test] [--get-only] [--out OUT]
                       [--delete-remote] [--args ARGS]
                       cfg_file label [-- extra-args]

Schedule remote commands over SSH

positional arguments:
  cfg_file         .yml cfg file
  label            Label for resulting logs

optional arguments:
  -h, --help       show this help message and exit
  --parse-test     Only test parsing of cfg
  --get-only       Only get log files, do not run
  --out OUT        Directory to output files into
  --delete-remote  Deletes remote log directories
  --args ARGS      Additional arguments which are passed to the config file
                   (format 'k1:v1;k2:v2').
                   Arguments may also be passed as flags following a '--'
                   (e.g. -- --k1=v1 --k2=v2)
```

## Basic Configuration Format

The simplest Shremote config file is simply a list of commands,
and the hosts on which they are to run:

```yaml
commands:
    # This program will execute on host1.net at time = 0 seconds (immediately)
    - program:
        start: echo "Hello World on Host 1"
        hosts:
            addr: host1.net
      begin: 0

    # This program will execute on host2.net at time = 10 seconds
    - program:
        start: echo "Hello World on Host 2"
        hosts:
            addr: host2.net
      begin: 10
```

Programs can also be be separated so that they can be referred to by name and reused:

```yaml
programs:
    echo:
        start: echo {to_echo}
        # {to_echo} will be substituted later, within the command
    log:
        out: "echo_output.txt"
        # stdout will be logged to this file,
        # and collected automatically at the end

commands:
    - program: echo
      hosts:
        addr: host1.net
      to_echo: "Hello World with a reused program on host 1"
      begin: 0

    - program: echo
      hosts:
        addr: host2.net
      to_echo: "Hello World wiht a reused program on host 2"
      begin: 0
```

Similarly, hosts can be separated for reuse

```yaml
hosts:
    host1:
        addr: host1.net
        ssh: # A host can also specify additional ssh information
            user: my_username
            key: ~/.ssh/my_sshkey
            port: 1234

programs:
    echo:
        start: echo {to_echo}
        hosts: host1
        # Host can be specified in either a program, or a command

commands:
    - program: echo
      to_echo: "Hello world: multiple levels of indirection!"
      begin: 0
```

This much should be enough to run most tests!

However, Shremote has many more capabilities,
which are described in detail below.

## Full Configuration Format

The [configuration file format](shlib/cfg_format.py)
lists all of the fields which are available, referencable, or required,
in the configuration file.

The following is an overview of the configuration format and capabilities

### Capabilities

#### References
Certain fields in the config may be referred to by using their key.
This allows for the reuse of `host`s and `program`s within and throughout
config files.

For example, a program may refer to its host by name, or it may define the host inline.
Similarly, a command may specify the program by name, or define it inline.

If a field is defined inline, it must match the format specifications
of the field it refers to.

(e.g. a program's inline `host` must meet the requirements of a top-level specified `host`)

The fields which may use references are of type CfgReference in the [format_spec](shlib/cfg_format.py)

Example: [references.yml](examples/references.yml)

#### Substitutions
Throughout the config file, remote sections of the config file may
referenced and inserted into most fields.

String formatting takes place via pythons `.format()` method.
The root of the config file is always passed in as the first argument
to `.format()`, so `{0.a}` is expanded into the contents of the
field `a` at the root of the config.

Certain fields
(marked with the flag `format_root` in the [format spec](cfg_format.yml)
are also passed as  keyword arguments to `.format()`

For example, a program may specify a start string `{foo}`, if the
command which uses that program defines a field of that name:

```yaml
program:
    echo:
        start: echo "{foo}"

commands:
    - program: echo
      foo: bar
```
In this example, the utilized command will be `echo "bar".

Example: [substitutions.yml](examples/substitutions.yml)

##### Computed fields
Certain substitutable fields are dynamic, and thus are not directly
defined by the config file.
Those fields are provided by shremote at runtime, and may differ
between executions of a command.

For example:
* `{0.user}` will be substituted with the name of the user executing Shremote.
* `{0.label}` will be substituted with the label provided to shremote on execution.
* `{0.args.foo}` will be substituted with the value of the argument `foo`,
which must have been provided by the user from the command line, such as:
`--args="foo:bar"`, which will substitute `{0.args.foo}` for `bar`.
* `{host.addr}` (within a command or program)
will be substituted for the address of the host on which
the currently executing command is running.
* `{host.log_dir}` will be substituted for the directory
into which logs are placed on a remote host

Example: [computed_fields.yml](examples/computed_fields.yml)

#### Evaluation
Strings, or portions of strings, placed within `$(...)$` will be run through
python's `eval()` function before being used.

This can be used to compute the time at which command should start,
or to perform simple string substitution.

E.g. `$(x + "y" * 5 + z)$` will evaluate to `"xyyyyyz"`, and `$(40 + 2)$`
will evaluate to `42`.

References may also be nested within evaluations.

In addition to typical python functionality, four special functions are
provided for use within `$(...)$` blocks:

```yaml
# Get a command line argument, providing a default value
# e.g. if shremote was initialized with --key=value: 'value'
#      otherwise: 'default_value'
x: $( getarg('key', 'default_value') )$

# Check for the existence of a command line argument
x: $( 1 if hasarg('key') else 2)$
# or
enabled: $(hasarg('use_program'))$

#  Pass through an argument from the command line
# e.g. if shremote was initialized with --context=3,
#       this will pass --context=3 to grep. Otherwise it is ignored
start: grep $( passarg('context') )$

# Prompt the user for a password at program initialization.
# Useful especially for sudo access
my_host:
    sudo_password: $( askpass('host_id') )$
# 'host_id' is an identifier, which allows you to use the same prompted
# password in multiple locations without reprompting
```

Example: [evaluated_fields.yml](examples/evaluated_fields.yml)

#### Inclusion
One yaml file, or parts of a yaml file, may be included in another
in one of three ways:

**Inclusion**: Copy the entire file into the specified map
```yaml
included_section: !include file/to/include.yml
```

**Importing**: Copy a section of the included file.
In this case, copies `field.to.include` into `imported_section`
```yaml
imported_section: !import included/file.yml::field::to::include
```

**Inheritance**: Copies a section included file, but lets you override fields of it.
This syntax is weird, and in no way is standard yaml.
The first line following the `|` provides the file/section to include.
Following lines override fields in included section.
```yaml
inherited_section: !inherit |
    sample_cfgs/includable.yml::entry_one
    sub_key2: new_value2
```

### Specifications

Though the config file may contain as many fields as are useful for the
task at hand, certain fields are used throughout the program.
Of these, only `commands` is mandatory, but if others are present
they must follow specified formats.

The following are the utilized fields at the root level of the config:

##### `log_dir`
The directory into which logs are placed on remote hosts.

* **Default**: `~/shremote_logs`
* **Overridden by**: `hosts.<host>.log_dir`

##### `ssh`
The parameters passed to ssh calls for connecting to remote hosts.

* **Fields**
  * `user`: defaults to `{0.user}`
  * `key`: ssh key to use. Defaults to `~/.ssh/id_rsa`
  * `port`: defaults to 22
* **Overridden by**: `host.<host>.ssh`

##### `init_cmds`/`post_cmds`
These commands are executed **on the local host** at the start/end of execution.

For `init_cmds`, before files are copied or any remote calls are made.
For `post_cmds`, after files are copied back to the local host

* **Format**: List of maps
* **Fields**:
  * `cmd`: A string with the command to execute
  * `checked_rtn`: If an integer, a non-matching return code will abort execution. Otherwise, set to `null`

##### `hosts`
Remote hosts on which to execute commands

* **Format**: Map of maps
* **Fields**:
  * `name`: (automatically filled) Defaults to the key that defines this host
  * `hostname`: The address (Hostname or IP) to use for ssh'ing
  * `ssh`: (optional) overrides `ssh` above
  * `log_dir`: (optional) Overrides `log_dir` above
  * `sudo_passwd`: (optional) Provide a password to use when running `sudo` commands on that host. It is suggested to use the value `$( askpass('<hostname>') )$` to avoid storing passwords in the config
* **Computed Fields**:
  * `output_dir`: The directory to which this experiment's logs are output on this host
  (`{log_dir}/{0.label}`)
* **Referenced by**: `files.<file>.hosts`, `programs.<program>.hosts`, `commands.<command>.hosts`

##### `files`
Specifies files to be copied from the local host to remote hosts

* **Format**: Map of maps
* **Fields**:
  * `name`: (automatically filled) defaults to the key that defines this file
  * `src`: The source location of the file on the local host
  * `dst`: The destination of the file on the remote host
  * `hosts`: Host(s) (or references to host(s)) onto which the file should be copied
  * `enabled`: (optional) If False, this file will not be copied
* **Computed fields**:
  * `host`: If copying to multiple hosts, the config map of the current host.
  Thus `{host.output_dir}` references the output directory on the remote host
* **Referenced by**: `commands.<command>.program`

##### `programs`
Specifies programs to be executed on remote hosts.

* **Format**: Map of maps
* **Fields**:
  * `name`: (automatically filled) defaults to the key that defines this program
  * `hosts`: Host(s) (or references to host(s)) on which the program should be executed.
  Optional, and overridden by `hosts` in a `command` (if present).
  * `start`: The shell command used to start this program
  * `stop`: (optional) The shell command used to stop this program. `kill {pid}` will often be appropriate.
  * `duration_reduced_error`: (optional) If `true`, shremote will throw an error if the program executes for shorter than the command's `min_duration`. Defaults to `true`.
  * `duration_exceeded_error`: (optional) If `true`, shremote will throw an error if the program lasts longer than the command's `max_duration`. Defaults to `false`.
  * `bg`: (optional) If `true`, the command will run in the background, and will have to be manually stopped with the `stop` command
  * `defaults`: (optional) Provides default arguments for string substitution. Overridden by fields in the `command`
  * `checked_rtn`: (optional) If provided, shremote will stop execution if the return value of the command to start the program is not equal to this value (generally `0`)
  * `sudo`: (optional) If True, shremote will automatically enter in the host's `sudo_passwd` field to stdin when running the program. This field will not function properly if `sudo` is not used exactly one time in the `start` command.
  * log: A map containing:
    * `dir`: (optional) A sub-directory into which logs should be placed
    * `out`: (optional) A file to which stdout should be logged
    * `err`: (optional) A file to which stderr should be logged
* **Computed fields**: See `commands.computed_fields` below

##### `commands`
Specifies when and where to execute commands

* **Format**: List of maps
* **Fields**:
  * `program`: Program (or reference to program) to execute
  * `name`: (automatically filled) defaults to the program name
  * `hosts`: Host(s) (or references to host(s)) on which to execute the program. Overrides any host defined within the program.
  * `begin`: The time, relative to the start of the experiment, at which to run the command
  * `min_duration`: The minimum duration to run the command for. Only has an effect if `program.duration_reduced_error` is set to true
  * `max_duration`: The maximum duration for which the command is to be run. The command is killed after this duration is exceeded. If `program.duration_exceeded_error` is `true`, reaching this duration will raise an error
  * `start_after`: If present, this command will start following the end of the command with the given name
  * `stop_after`: If present, this command will stop following the end of the command with the given name
  * `enabled`: (optional) If False, the command will not be run
* **Computed fields**:
  * `host_idx`: If executing on multiple hosts, the index of the currently-executing host
  * `log_dir`: The full path to the log directory for this program (including `<program>.log.dir`) on the active host.
  * `host`: If executing on multiple hosts, the config map of the host on which the command is currently being executed
  * `pid`: The process ID of the running command, once started. To be used in the "stop" section of a program.

##### Root-level computed fields
These fields are available with `{0.<field>}` throughout the config

* `user`: The user currently executing the config
* `label`: The label provided on starting Shremote
* `output_dir`: The directory that logs will be copied to on the local machine
* `cfg_dir`: The directory in which the config file resides
* `args`: A map of the key-value argument provided in the `--args` argument at startup.
  e.g. if the args string is `--args "key_a:value_b"`, or the program was started with `-- --key_a=value_b`, the string `{0.args.key_a}` will be replaced by `value_b`
