# fileq

A simple utility that organizes a FIFO queue for simple text messages via a file.

## Install

```bash
$ make
$ make install
```

## Usage

```bash
$ fq <command> [-q <queue_name> [<message>]]
```

Commands:

- `help` displays help message
- `list` shows the names of existing queues
- `show -q <queue_name>` show content (all messages) of the queue; messages will not be removed from the queue
- `size -q <queue_name>` show the size (number of messages) of the queue
- `pull -q <queue_name>` get the first (oldest) message from the queue
- `push -q <queue_name> <message>` add a new message to the queue
