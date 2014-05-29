// **********************************************************************
//
// Copyright (c) 2003-2014 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

package IceInternal;

public final class Selector
{
    static final class TimeoutException extends Exception
    {
    }

    public Selector(Instance instance)
    {
        _instance = instance;
        _selecting = false;
        _interrupted = false;

        try
        {
            _selector = java.nio.channels.Selector.open();
        }
        catch(java.io.IOException ex)
        {
            throw new Ice.SyscallException(ex);
        }

        //
        // The Selector holds a Set representing the selected keys. The
        // Set reference doesn't change, so we obtain it once here.
        //
        _keys = _selector.selectedKeys();
    }

    public void destroy()
    {
        try
        {
            _selector.close();
        }
        catch(java.io.IOException ex)
        {
        }
        _selector = null;
    }

    public void initialize(EventHandler handler)
    {
        updateImpl(handler);
    }

    public void update(EventHandler handler, int remove, int add)
    {
        int previous = handler._registered;
        handler._registered = handler._registered & ~remove;
        handler._registered = handler._registered | add;
        if(previous == handler._registered)
        {
            return;
        }

        updateImpl(handler);
    }

    public void enable(EventHandler handler, int status)
    {
        if((handler._disabled & status) == 0)
        {
            return;
        }
        handler._disabled = handler._disabled & ~status;

        if((handler._registered & status) != 0)
        {
            updateImpl(handler);
        }
    }

    public void disable(EventHandler handler, int status)
    {
        if((handler._disabled & status) != 0)
        {
            return;
        }
        handler._disabled = handler._disabled | status;

        if((handler._registered & status) != 0)
        {
            updateImpl(handler);
        }
    }

    public void finish(EventHandler handler)
    {
        if(handler._registered != 0)
        {
            if(handler._key != null)
            {
                handler._key.cancel();
                handler._key = null;
            }

            _changes.remove(handler);

            update(handler, handler._registered, SocketOperation.None);
        }
    }

    public void startSelect()
    {
        if(_interrupted)
        {
            _interrupted = false;

            if(!_changes.isEmpty())
            {
                updateSelector();
            }
        }
        _selecting = true;
    }

    public void finishSelect()
    {
        _selecting = false;
    }

    public void select(java.util.List<EventHandlerOpPair> handlers, long timeout)
        throws TimeoutException
    {
        while(true)
        {
            try
            {
                if(timeout > 0)
                {
                    //
                    // NOTE: On some platforms, select() sometime returns slightly before
                    // the timeout (at least according to our monotonic time). To make sure
                    // timeouts are correctly detected, we wait for a little longer than
                    // the configured timeout (10ms).
                    //
                    long before = IceInternal.Time.currentMonotonicTimeMillis();
                    if(_selector.select(timeout * 1000 + 10) == 0)
                    {
                        if(IceInternal.Time.currentMonotonicTimeMillis() - before >= timeout * 1000)
                        {
                            throw new TimeoutException();
                        }
                    }
                }
                else
                {
                    _selector.select();
                }
            }
            catch(java.nio.channels.CancelledKeyException ex)
            {
                // This sometime occurs on OS X, ignore.
                continue;
            }
            catch(java.io.IOException ex)
            {
                //
                // Pressing Ctrl-C causes select() to raise an
                // IOException, which seems like a JDK bug. We trap
                // for that special case here and ignore it.
                // Hopefully we're not masking something important!
                //
                if(Network.interrupted(ex))
                {
                    continue;
                }

                try
                {
                    String s = "fatal error: selector failed:\n" + ex.getCause().getMessage();
                    _instance.initializationData().logger.error(s);
                }
                finally
                {
                    Runtime.getRuntime().halt(1);
                }
            }

            break;
        }

        handlers.clear();

        if(_interrupted) // Interrupted, we have to process the interrupt before returning any handlers
        {
            return;
        }

        if(_keys.isEmpty() && timeout <= 0)
        {
            //
            // This is necessary to prevent a busy loop in case of a spurious wake-up which
            // sometime occurs in the client thread pool when the communicator is destroyed.
            // If there are too many successive spurious wake-ups, we log an error.
            //
            try
            {
                Thread.sleep(1);
            }
            catch(java.lang.InterruptedException ex)
            {
            }

            if(++_spuriousWakeUp > 100)
            {
                _spuriousWakeUp = 0;
                _instance.initializationData().logger.warning("spurious selector wake up");
            }
            return;
        }

        _spuriousWakeUp = 0;

        for(java.nio.channels.SelectionKey key : _keys)
        {
            EventHandler handler = (EventHandler)key.attachment();
            try
            {
                //
                // Use the intersection of readyOps and interestOps because we only want to
                // report the operations in which the handler is still interested.
                //
                final int op = fromJavaOps(key.readyOps() & key.interestOps());
                handlers.add(new EventHandlerOpPair(handler, op));
            }
            catch(java.nio.channels.CancelledKeyException ex)
            {
                assert(handler._registered == 0);
            }
        }
        _keys.clear();
    }

    private void updateImpl(EventHandler handler)
    {
        _changes.add(handler);
        if(_selecting)
        {
            if(!_interrupted)
            {
                //
                // We can't change the selection key interest ops while a select operation is in progress
                // (it could block depending on the underlying implementation of the Java selector).
                //
                // Wake up the selector if necessary.
                //
                _selector.wakeup();
                _interrupted = true;
            }
        }
        else
        {
            updateSelector();
        }
    }

    private void updateSelector()
    {
        for(EventHandler handler : _changes)
        {
            int status = handler._registered & ~handler._disabled;
            int ops = toJavaOps(handler, status);
            if(handler._key == null)
            {
                if(handler._registered != 0)
                {
                    try
                    {
                        handler._key = handler.fd().register(_selector, ops, handler);
                    }
                    catch(java.nio.channels.ClosedChannelException ex)
                    {
                        assert(false);
                    }
                }
            }
            else
            {
                handler._key.interestOps(ops);
            }
        }
        _changes.clear();
    }

    int toJavaOps(EventHandler handler, int o)
    {
        int op = 0;
        if((o & SocketOperation.Read) != 0)
        {
            if((handler.fd().validOps() & java.nio.channels.SelectionKey.OP_READ) != 0)
            {
                op |= java.nio.channels.SelectionKey.OP_READ;
            }
            else
            {
                op |= java.nio.channels.SelectionKey.OP_ACCEPT;
            }
        }
        if((o & SocketOperation.Write) != 0)
        {
            op |= java.nio.channels.SelectionKey.OP_WRITE;
        }
        if((o & SocketOperation.Connect) != 0)
        {
            op |= java.nio.channels.SelectionKey.OP_CONNECT;
        }
        return op;
    }

    int fromJavaOps(int o)
    {
        int op = 0;
        if((o & (java.nio.channels.SelectionKey.OP_READ | java.nio.channels.SelectionKey.OP_ACCEPT)) != 0)
        {
            op |= SocketOperation.Read;
        }
        if((o & java.nio.channels.SelectionKey.OP_WRITE) != 0)
        {
            op |= SocketOperation.Write;
        }
        if((o & java.nio.channels.SelectionKey.OP_CONNECT) != 0)
        {
            op |= SocketOperation.Connect;
        }
        return op;
    }

    final private Instance _instance;

    private java.nio.channels.Selector _selector;

    private java.util.Set<java.nio.channels.SelectionKey> _keys;
    private java.util.HashSet<EventHandler> _changes = new java.util.HashSet<EventHandler>();
    private java.util.HashSet<EventHandler> _pendingHandlers = new java.util.HashSet<EventHandler>();
    private boolean _selecting;
    private boolean _interrupted;
    private int _spuriousWakeUp;
}
