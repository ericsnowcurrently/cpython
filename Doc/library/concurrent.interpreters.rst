:mod:`!concurrent.interpreters` --- Multiple interpreters in the same process
=============================================================================

.. module:: concurrent.interpreters
   :synopsis: Multiple interpreters in the same process

.. versionadded:: 3.14

**Source code:** :source:`Lib/concurrent/interpreters`

--------------

The :mod:`!concurrent.interpreters` module constructs higher-level
interfaces on top of the lower level :mod:`!_interpreters` module.

The module is primarily meant to provide a basic API for managing
interpreters (AKA "subinterpreters") and running things in them.
Running mostly involves switching to an interpreter (in the current
thread) and calling a function in that execution context.

For concurrency, interpreters themselves (and this module) don't
provide much more than isolation, which on its own isn't useful.
Actual concurrency is available separately through
:mod:`threads <threading>`  See `below <interp-concurrency_>`_

.. seealso::

   :class:`~concurrent.futures.InterpreterPoolExecutor`
      Combines threads with interpreters in a familiar interface.

   .. XXX Add references to the upcoming HOWTO docs in the seealso block.

   :ref:`isolating-extensions-howto`
      How to update an extension module to support multiple interpreters.

   :pep:`554`

   :pep:`734`

   :pep:`684`

.. XXX Why do we disallow multiple interpreters on WASM?

.. include:: ../includes/wasm-notavail.rst


Key details
-----------

Before we dive in further, there are a small number of details
to keep in mind about using multiple interpreters:

* `isolated <interp-isolation_>`_, by default
* no implicit threads
* not all PyPI packages support use in multiple interpreters yet

.. XXX Are there other relevant details to list?


.. _interpreters-intro:

Introduction
------------

An "interpreter" is effectively the execution context of the Python
runtime.  It contains all of the state the runtime needs to execute
a program.  This includes things like the import state and builtins.
(Each thread, even if there's only the main thread, has some extra
runtime state, in addition to the current interpreter, related to
the current exception and the bytecode eval loop.)

The concept and functionality of the interpreter have been a part of
Python since version 2.2, but the feature was only available through
the C-API and not well known, and the `isolation <interp-isolation_>`_
was relatively incomplete until version 3.12.

.. _interp-isolation:

Multiple Interpreters and Isolation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A Python implementation may support using multiple interpreters in the
same process.  CPython has this support.  Each interpreter is
effectively isolated from the others (with a limited number of
carefully managed process-global exceptions to the rule).

That isolation is primarily useful as a strong separation between
distinct logical components of a program, where you want to have
careful control of how those components interact.

.. note::

   Interpreters in the same process can technically never be strictly
   isolated from one another since there are few restrictions on memory
   access within the same process.  The Python runtime makes a best
   effort at isolation but extension modules may easily violate that.
   Therefore, do not use multiple interpreters in security-sensitive
   situations, where they shouldn't have access to each other's data.

Running in an Interpreter
^^^^^^^^^^^^^^^^^^^^^^^^^

Running in a different interpreter involves switching to it in the
current thread and then calling some function.  The runtime will
execute the function using the current interpreter's state.  The
:mod:`!concurrent.interpreters` module provides a basic API for
creating and managing interpreters, as well as the switch-and-call
operation.

No other threads are automatically started for the operation.
There is `a helper <interp-call-in-thread_>`_ for that though.
There is another dedicated helper for calling the builtin
:func:`exec` in an interpreter.

When :func:`exec` (or :func:`eval`) are called in an interpreter,
they run using the interpreter's :mod:`!__main__` module as the
"globals" namespace.  The same is true for functions that aren't
associated with any module.  This is the same as how scripts invoked
from the command-line run in the :mod:`!__main__` module.


.. _interp-concurrency:

Concurrency and Parallelism
^^^^^^^^^^^^^^^^^^^^^^^^^^^

As noted earlier, interpreters do not provide any concurrency
on their own.  They strictly represent the isolated execution
context the runtime will use *in the current thread*.  That isolation
makes them similar to processes, but they still enjoy in-process
efficiency, like threads.

All that said, interpreters do naturally support certain flavors of
concurrency.
There's a powerful side effect of that isolation.  It enables a
different approach to concurrency than you can take with async or
threads.  It's a similar concurrency model to CSP or the actor model,
a model which is relatively easy to reason about.

You can take advantage of that concurrency model in a single thread,
switching back and forth between interpreters, Stackless-style.
However, this model is more useful when you combine interpreters
with multiple threads.  This mostly involves starting a new thread,
where you switch to another interpreter and run what you want there.

Each actual thread in Python, even if you're only running in the main
thread, has its own *current* execution context.  Multiple threads can
use the same interpreter or different ones.

At a high level, you can think of the combination of threads and
interpreters as threads with opt-in sharing.

As a significant bonus, interpreters are sufficiently isolated that
they do not share the :term:`GIL`, which means combining threads with
multiple interpreters enables full multi-core parallelism.
(This has been the case since Python 3.12.)

Communication Between Interpreters
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In practice, multiple interpreters are useful only if we have a way
to communicate between them.  This usually involves some form of
message passing, but can even mean sharing data in some carefully
managed way.

With this in mind, the :mod:`!concurrent.interpreters` module provides
a :class:`queue.Queue` implementation, available through
:func:`create_queue`.

.. _interp-object-sharing:

"Sharing" Objects
^^^^^^^^^^^^^^^^^

"Sharing" between interpreters refers to sending data *represented* by
objects.  It does not mean multiple interpreters are using the same
object at the same time (with a very small number of exceptional cases).

Actually sharing objects (or other data) between interpreters is tricky.
Any data actually shared between interpreters loses the consistency
and thread-safety provided by the isolation of each interpreter,
particularly the :term:`GIL`.

Thus, sharing *mutable* data requires some coordination or frequent
synchronization and context switching.  That can quickly make it
expensive.  Any data tied to one interpreter's lifetime, even immutable
data, faces similar complexity.

Python objects, with very few exceptions, are always mutable and always
tied to the interpreter where they were created.  Therefore they can't
be truly shared without significant extra cost.

Consequently, sharing objects and other data between interpreters must
be done thoughtfully and carefully, even if only for short communication
exchanges.  There are several effective approaches to doing so.

Pure Python code doesn't have the capacity to accomplish any of these,
at least not fully.  Instead, the :mod:`!concurrent.interpreters` module
and the runtime provide help.

The simplest approach is to make a copy of the original data for use
in a target interpreter.  This often involves some form of
serialization.  This *can* be done efficiently, especially by the Python
runtime and within extension modules, but not always.  Most notably,
copying means mutable objects won't stay in sync.

When the shared data is especially large, it often makes sense to
actually share it, such as through the
:ref:`buffer protocol <python-buffer-protocol>`.  Then the interpreters
must cooperate on reading and writing, whether through locks or
assigning partitions or something else.

Regardless of the approach, nearly all "sharing" between interpreters
involves brief communication exchanges.  The data is almost always
shared only for the duration of the exchange.

In general, the exchange between interpreters looks like the following:

1. convert an object to raw, cross-interpreter safe data
2. pass the data to another interpreter
3. convert the data back into an object

The runtime provides support for those steps.  Pure Python code has
access to that support through the :mod:`!concurrent.interpreters`
module.  For example, both :meth:`Interpreter.prepare_main` and
:class:`Queue.put() <Queue>` implement the exchange.
:func:`as_shareable` can help with the two conversion steps.

.. _xidata-protocol:

Cross-interpreter Data Protocol
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The runtime support for the two conversion steps is low-level and not
directly available to pure Python code.  However, there's a standardized
protocol that fills the gap.

.. XXX Add link to C-API?

At the low level, some builtin types and some types in extension modules
provide direct support to the Python runtime for the conversion steps.
In fact, nearly all of the immutable builtin objects are either
directly shared or copied efficiently.  For example:

* :const:`None`
* :class:`bool` (:const:`True` and :const:`False`)
* :class:`bytes`
* :class:`str`
* :class:`int`
* :class:`float`
* :class:`tuple` (of similarly supported objects)

There is a small number of builtin and stdlib types that actually share
mutable data between interpreters:

* :class:`memoryview`
* :class:`Queue`

Most objects do not have that low-level support, including instances
of all pure Python classes.  Such objects cannot be shared without some
other mechanism to manage the conversion, such as the default fallback
of :meth:`Interpreter.prepare_main`.  Aside from such fallbacks, the
standardized way for code to bridge that gap is with a
"conversion function".

A conversion function takes an object and returns an object that
supports low-level conversion.  It also returns a "restore" function
that turns the converted object back into an equivalent of the original.
(In some uncommon cases, the restore function may be ``None``, meaning
the converted object should be used as-is.)

The restore function will be converted to cross-interpreter data and
back during the exchange, so the function must be stateless.  This
means it cannot rely on external state nor on internal state.  It may
rely on arguments and builtins but not on globals or a closure.
A stateless function is equivalent to its code object.

The the sharing-related functions and methods in
:mod:`!concurrent.interpreters` all support one conversion function in
particular: the :meth:`~object.__xidata__` special method.  Classes
that do not provide a low-level implementation for sharing may still
control how they are shared by implementing it.

.. XXX Add an entry for __xidata__ in Doc/reference/datamodel.rst?
   (right before :ref:`python-buffer-protocol`?)

.. method:: object.__xidata__(self)

   Converts the object to one that is sharing-compatible.  Returns
   ``(converted, restore)``, where "converted" is the object to "share".

   For classes that directly support conversion to cross-interpreter
   data, ``__xidata__()`` returns ``(self, None)``.

   .. versionadded:: 3.16

Cross-interpreter compatibility of an object's type can be checked with
:func:`is_shareable`.  Conversion can be done ahead of time with
:func:`as_shareable`.  :class:`CrossInterpreterObjectData` acts
as a wrapper around the raw data.


Reference
---------

This module defines the following functions:

.. function:: list_all()

   Return a :class:`list` of :class:`Interpreter` objects,
   one for each existing interpreter.

.. function:: get_current()

   Return an :class:`Interpreter` object for the currently running
   interpreter.

.. function:: get_main()

   Return an :class:`Interpreter` object for the main interpreter.
   This is the interpreter the runtime created to run the :term:`REPL`
   or the script given at the command-line.  It is usually the only one.

.. function:: create()

   Initialize a new (idle) Python interpreter
   and return a :class:`Interpreter` object for it.

.. function:: create_queue()

   Initialize a new cross-interpreter queue and return a :class:`Queue`
   object for it.

.. function:: is_shareable(obj)

   Return True is the object's type supports the "sharing" protocol.

   Note that this is a fast, trivial check and does not check the
   object's value.  While a False result is always correct, a True
   result could be incorrect if the object's value where factored in.

   This is because some types support cross-interpreter data only for
   a subset of their instances.  For example, ``is_shareable()``
   returns True for a tuple even if it contains objects
   that are not shareable.

   For a complete result, use :func:`as_shareable`.

.. function:: as_shareable(obj, convert=None, \*, fallback=None)

   Return a :class:`CrossInterpreterObjectData` that wraps the raw
   shareable data for the object.  This relies on the same protocol
   as :meth:`Interpreter.prepare_main` and :class:`Queue` and uses
   the same mechanism.  The main difference is that, by default,
   this does not use any fallback other than
   :meth:`~object.__xidata__`.

   If the given object cannot be converted to with sharable data then,
   by default, ``as_shareable()`` raises :exc:`NotShareableError`.
   This can be avoided by providing a ``convert`` function
   or a fallback value.

   "convert" must be a "stateless" function that takes an object
   and returns ``(converted obj, restore func)``.  See
   :ref:`xidata-protocol` for more information.

   "fallback" indicates what to do if the object (or the converted
   object when "convert" is provided) is not sharing-compatible.
   If ``True`` (or ``None``) then it only falls back to the type's
   ``__xidata__()`` method, if available, as an implicit convert
   func.  It may also be "full", which means it will do what
   :meth:`Interpreter.prepare_main` and :class:`Queue`
   do by default: try ``__xidata__()``, as a function, and then
   serialize with pickle.

   If "obj" is a :class:`CrossInterpreterObjectData` then it is
   returned as-is.

   .. versionadded:: 3.16


CrossInterpreterData objects
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. class:: CrossInterpreterData

   A wrapper around raw data that may be safely shared between
   interpreters.

   .. XXX Drop the restriction?

   Note that the wrapper object may be passed to a sharing
   operation, like :meth:`Interpreter.prepare_main` only once, to
   avoid ...

   .. attribute:: iswrapped

      (read-only)

      Returns ``True`` if the original object was converted to another.
      If so, :meth:`as_object` and :meth:`as_wrapped_object` will
      return different objects.

   .. method:: as_object(self)

      Returns an object corresponding to the data, applying any
      unwrapper as necessary.  This essentially restores the
      original object.

   .. method:: as_wrapped_object(self)

      Returns (obj, unwrap) if the original object was "wrapped".
      "obj" is the actual object that was converted to cross-interpreter
      data, corresponding to the result of wrapping the original object.
      "unwrap" is the function to get the object corresponding to the
      original, as described for :func:`as_shareable`.

      Returns (obj, None) if the original object was not wrapped,
      the same as :meth:`as_object`.

.. versionadded:: 3.16


Interpreter objects
^^^^^^^^^^^^^^^^^^^

.. class:: Interpreter(id)

   A single interpreter in the current process.

   Generally, :class:`Interpreter` shouldn't be called directly.
   Instead, use :func:`create` or one of the other module functions.

   .. attribute:: id

      (read-only)

      The underlying interpreter's ID.

   .. attribute:: whence

      (read-only)

      A string describing where the interpreter came from.

   .. method:: is_running()

      Return ``True`` if the interpreter is currently executing code
      in its :mod:`!__main__` module and ``False`` otherwise.

   .. method:: close()

      Finalize and destroy the interpreter.

   .. method:: prepare_main(ns=None, **kwargs)

      Bind objects in the interpreter's :mod:`!__main__` module.

      Some objects are actually shared and some are copied efficiently,
      but most are copied via :mod:`pickle`.  See :ref:`interp-object-sharing`.

   .. method:: exec(code, /, dedent=True)

      Run the given source code in the interpreter (in the current thread).

   .. method:: call(callable, /, *args, **kwargs)

      Return the result of calling running the given function in the
      interpreter (in the current thread).

   .. _interp-call-in-thread:

   .. method:: call_in_thread(callable, /, *args, **kwargs)

      Run the given function in the interpreter (in a new thread).

Exceptions
^^^^^^^^^^

.. exception:: InterpreterError

   This exception, a subclass of :exc:`Exception`, is raised when
   an interpreter-related error happens.

.. exception:: InterpreterNotFoundError

   This exception, a subclass of :exc:`InterpreterError`, is raised when
   the targeted interpreter no longer exists.

.. exception:: ExecutionFailed

   This exception, a subclass of :exc:`InterpreterError`, is raised when
   the running code raised an uncaught exception.

   .. attribute:: excinfo

      A basic snapshot of the exception raised in the other interpreter.

.. XXX Document the excinfoattrs?

.. exception:: NotShareableError

   This exception, a subclass of :exc:`TypeError`, is raised when
   an object cannot be sent to another interpreter.


Communicating Between Interpreters
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. class:: Queue(id)

   A wrapper around a low-level, cross-interpreter queue, which
   implements the :class:`queue.Queue` interface.  The underlying queue
   can only be created through :func:`create_queue`.

   Some objects are actually shared and some are copied efficiently,
   but most are copied via :mod:`pickle`.  See :ref:`interp-object-sharing`.

   .. attribute:: id

      (read-only)

      The queue's ID.


.. exception:: QueueEmptyError

   This exception, a subclass of :exc:`queue.Empty`, is raised from
   :meth:`!Queue.get` and :meth:`!Queue.get_nowait` when the queue
   is empty.

.. exception:: QueueFullError

   This exception, a subclass of :exc:`queue.Full`, is raised from
   :meth:`!Queue.put` and :meth:`!Queue.put_nowait` when the queue
   is full.


Basic usage
-----------

Creating an interpreter and running code in it::

    from concurrent import interpreters

    interp = interpreters.create()

    # Run in the current OS thread.

    interp.exec('print("spam!")')

    interp.exec("""if True:
        print('spam!')
        """)

    from textwrap import dedent
    interp.exec(dedent("""
        print('spam!')
        """))

    def run(arg):
        return arg

    res = interp.call(run, 'spam!')
    print(res)

    def run():
        print('spam!')

    interp.call(run)

    # Run in new OS thread.

    t = interp.call_in_thread(run)
    t.join()
