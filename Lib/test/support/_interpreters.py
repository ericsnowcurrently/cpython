import time
import _xxsubinterpreters as _interpreters


def is_shareable(obj):
    """Return True if the object may be "shared" with other interpreters.

    Note that when a Python object is "shared", the receiving
    interpreter actually gets a new object derived from the original
    object's data.  Due to interpreter isolation, using the same object
    in multiple interpreters leads to undefined behavior.

    So, for the sake of efficiency and interpreter isolation,
    "shareable" objects are typically immutable objects that serialize
    to small amounts of data.  Not all objects are shareable.  A class
    must opt in to being "shareable" (in CPython: via the C-API).
    Examples of shareable objects include str and None.
    """
    return _interpreters.is_shareable(obj)


class _LowLevelWrapper:

    def __init__(self, id):
        self.id = id

    def __repr__(self):
        return '{}(id={!r})'.format(
            type(self).__name__,
            int(self.id),
            )

    def __eq__(self, other):
        if type(other) is not type(self):
            return NotImplemented
        return self.id == other.id

    def __hash__(self):
        return hash(self.id)


##################################
# interpreters

RunFailedError = _interpreters.RunFailedError


def list_all():
    """Return a list of all interpreters in the current process."""
    return [Interpreter(id)
            for id in _interpreters.list_all()]


def get_current():
    """Return the currently running interpreter."""
    id = _interpreters.get_current()
    return Interpreter(id)


def get_main():
    """Return the main interpreter.

    The main interpreter is the one created by the Python runtime during
    initialization.  It will also be the last one left during runtime
    finalization (when the process ends).
    """
    id = _interpreters.get_main()
    return Interpreter(id)


def create():
    """Return a new interpreter.

    The caller is responsible for destroying the interpreter when done.
    """
    id = _interpreters.create()
    return Interpreter(id)


class Interpreter(_LowLevelWrapper):
    """A single Python interpreter in the current process."""

    def __init__(self, id):
        if not isinstance(id, _interpreters.InterpreterID):
            id = _interpreters.InterpreterID(id)
        super().__init__(id)

    def is_running(self):
        """Return True if the interpreter is currently executing code."""
        return _interpreters.is_running(self.id)

    def destroy(self):
        """Finaliza and delete the interpreter.

        A running interpreter may not be destroyed.  Consequently,
        calling destroy() on the current interpreter is an error.  So
        is destroying the main interpreter.
        """
        _interpreters.destroy(self.id)

    def run(self, code, *, channels=None):
        """Execute the given object in the interpreter.

        "code" is the object to run the the interpreter.  Currently
        only source code (str) is supported.

        "channels" is an optional mapping of identifiers to channel
        ends (RecvChannel/SendChannel).  The __main__ module of the
        interpreter will be updated with this mapping before execution
        begins.

        Calling run() on an already-running interpreter is an error.

        Execution in Current Thread
        ---------------------------

        The current Python thread will effectively block while the
        target interpreter executes the object in the current OS thread.
        Once execution ends, the Python thread of the original
        interpreter will resume running in the current OS thread.  This
        is effectively the same as any other blocking operation; only
        the current thread is involved and other threads of the original
        interpreter are not affected.

        Consequently, if the caller does not want the current thread
        to block then they should follow the existing pattern for
        calling blocking code: create a new thread in which to call
        run().

        The __main__ Module
        -------------------

        When an interpreter runs, it executes in the namespace of its
        __main__ module.  This module is created when the interpreter
        is created and is initially minimally populated.  Each time
        run() is called, the __main__ module is re-used as-is.

        run() never cleans up __main__, neither before execution nor
        after.  So anything added to the __main__ globals in one run()
        call will still be available in the __main__ globals during
        subsequent run() calls.  This behavior may be used to prepare
        an interpreter ahead of time (e.g. import modules, add channels)
        for later usage.

        If you want a fresh __main__ in a run() call, or do not want
        leave lingering data then you will need to either use a new
        interpreter or clean up the __main__ globals manually (in the
        code you passed to run()).

        Unhandled Exceptions
        --------------------

        Due to intepreter isolation, each unhandled exception during
        execution is caught in the target interpreter and converted to
        a RunFailedError object that represents the unhandled exception.
        This representation is lossy and a RunFailedError is not a
        faithful proxy for the original exception.

        So if information from that exception (e.g. type, attributes,
        traceback) is important, the caller should wrap their top-level
        code in a try-except block that preserves the information.  For
        example, the exception and traceback could be encoded into a
        string that is sent back via a channel.
        """
        if channels:
            for key, value in channels.items():
                if isinstance(value, (RecvChannel, SendChannel)):
                    channels[key] = value.id
        self._run_string(code, channels)

    def _run_string(self, src, channels):
        _interpreters.run_string(self.id, src, channels)


##################################
# channels

ChannelError = _interpreters.ChannelError
ChannelNotFoundError = _interpreters.ChannelNotFoundError
ChannelClosedError = _interpreters.ChannelClosedError
ChannelEmptyError = _interpreters.ChannelEmptyError


def list_all_channels():
    """Return a list of (recv, send) for all channels in the current process.

    Note that the list will include more channels than just the ones
    associated with the current interpreter.
    """
    return [(RecvChannel(id), SendChannel(id))
            for id in _interpreters.list_all_channels()]


def create_channel():
    """Create a new cross-interpreter object channel.

    A result of isolation between interpreters is that the a Python
    object may not be used in multiple interpreters at the same time.
    The benefits of using multiple interpreters are increased if there's
    a way to share data between interpreters.  A channel is the provided
    mechanism for "sharing" objects between interpreters.

    See "is_shareable()" for more on what "sharing" objects means.

    Associated Interpreters
    -----------------------

    A channels is a low-level entity that exists at the runtime level
    rather than on a per-interpreter basis.  However, knowing which
    interpreters are *using* a channel is useful.

    So the first time an interpreter uses a channel it gets associated
    with the channel (effectively added to a list of interpreters using
    the channel).  By "uses a channel" we mean "calls send() or recv()".
    Simply creating a channel or having a RecvChannel or SendChannel
    object does not count.  Interpreters are associated with either the
    recv end of the channel, the send end, or both.

    When a channel is closed (via RecvChannel.close() or
    SendChannel.close(), it gets un-associated with that end of the
    channel.  If the interpreter wasn't already associated then this
    is a noop (...unless the channel has never been associated with any
    interpreter.  In that case the channel gets closed).  If the
    interpreter was alraedy un-associated (via close()), or already
    completely closed, then ChannelClosedError is raised.  When the last
    associated interpreter is un-associated, then channel is closed and
    further use of the channel raises ChannelClosedError.
    """
    id = _interpreters.channel_create()
    return RecvChannel(id), SendChannel(id)


class ChannelEnd(_LowLevelWrapper):
    """The base class for RecvChannel and SendChannel."""

    def __init__(self, id, *, end):
        # XXX Optionally, get end from id.end?
        if end == 'recv':
            kwargs = dict(recv=True)
        elif end == 'send':
            kwargs = dict(send=True)
        else:
            raise ValueError('unsupported channel end {!r}'.format(end))
        id = _interpreters._channel_id(id, _resolve=True, **kwargs)
        super().__init__(id)
        self._kwargs = kwargs

    def close(self, *, force=False):
        """Drop the interpreter association with this end of the channel.

        Depending on how other interpreters are associated, this may
        close the channel to further operations.  See create_channel()
        for more about associated interpreters and how channels get
        closed.

        If "force" is True then the underlying channel will be closed
        for all interpreters.
        """
        # XXX Make close() a noop if already closed?  This would probably
        # mean adding an is_closed() method, a la Thread.is_alive().
        if force:
            _interpreters.channel_close(self.id)
        else:
            _interpreters.channel_drop_interpreter(self.id, **self._kwargs)


class RecvChannel(ChannelEnd):
    """The receiving end of a channel."""

    _sleep = time.sleep

    def __init__(self, id):
        super().__init__(id, end='recv')

    def recv(self):
        """Get the next object from the channel.

        If there aren't any objects then block until one is sent.

        Calling recv() associates the current interpreter with
        the recv end of the channel.  See create_channel() for more
        about associated interpreters.
        """
        # XXX Support a timeout?
        # XXX Handle blocking correctly in the low-level code.
        # This # can be done more efficiently there.  We also
        # want to make sure that concurrent recv() calls are
        # handled in the proper order.
        while True:
            try:
                return _interpreters.channel_recv(self.id)
            except ChannelEmptyError:
                self._sleep(0.1)  # seconds

    def recv_nowait(self):
        """Get the next object from the channel.

        If there aren't any objects then raise ChannelEmptyError.

        Calling recv_nowait() associates the current interpreter with
        the recv end of the channel.  See create_channel() for more
        about associated interpreters.
        """
        return _interpreters.channel_recv(self.id)


class SendChannel(ChannelEnd):
    """The sending end of a channel."""

    def __init__(self, id):
        super().__init__(id, end='send')

    def send(self, obj):
        """Push the object onto the channel's queue.

        send() blocks until the object is popped via the recv end of
        the channel.

        Calling send() associates the current interpreter with
        the send end of the channel.  See create_channel() for more
        about associated interpreters.
        """
        raise NotImplementedError

    def send_nowait(self, obj):
        """Push the object onto the channel's queue.

        send_nowait() returns immediately, rather than waiting for the
        object to be received.

        Calling send_nowait() associates the current interpreter with
        the send end of the channel.  See create_channel() for more
        about associated interpreters.
        """
        _interpreters.channel_send(self.id, obj)


del time
