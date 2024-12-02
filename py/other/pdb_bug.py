import sys

def _db():
    import socket
    # from here {

    # Leaving socket.socket.__repr__ as is messes the debugger in some way:
    #     $ py pdb_bug.py
    #     --Call--
    #     > /usr/lib64/python3.10/socket.py(243)__repr__()
    #     -> def __repr__(self):
    #     (Pdb) 
    # .. and hitting ^D doesn't raise like it should.

    # Having only this makes it behave normally in every way:
    #del socket.socket.__repr__

    # Using this shows an extra line, then behave normally:
    #     $ py pdb_bug.py
    #     
    #     --Return--
    #     > /data/sel/Documents/Projects/mopfiles/pdb_bug.py(39)<module>()->None
    #     -> breakpoint()
    #     (Pdb) 
    #     Traceback (most recent call last):
    #       File "/data/sel/Documents/Projects/mopfiles/pdb_bug.py", line 39, in <module>
    #         breakpoint()
    #       File "/usr/lib64/python3.10/bdb.py", line 94, in trace_dispatch
    #         return self.dispatch_return(frame, arg)
    #       File "/usr/lib64/python3.10/bdb.py", line 156, in dispatch_return
    #         if self.quitting: raise BdbQuit
    #     bdb.BdbQuit
    #socket.socket.__repr__ = print

    # It doesn't even have to be the initial __repr__, this has the first behavor too:
    #     py pdb_bug.py
    #     --Call--
    #     > /data/sel/Documents/Projects/mopfiles/pdb_bug.py(41)nonsense()
    #     -> def nonsense(self): ...
    #     (Pdb) 
    #def nonsense(self): ...
    #socket.socket.__repr__ = nonsense

    # But this behaves normally:
    #socket.socket.__repr__ = lambda: ""

    # } to here

    # Variable only needs to be named to cause issue,
    # that is calling the constructor by itself doesn't cause any issue:
    a = socket.socket()
    # If such a variable exists on the stack, as attribute to a wrapping class,
    # or even through a closure's captures; as long as an instance is reachable
    # the situation occures. Also note that using _socket.socket works fine.

    import pdb

    #pdb.set_trace()  # this is ok
    pdb.Pdb().set_trace(sys._getframe().f_back)  # this is not
    #(lambda: pdb.Pdb().set_trace(sys._getframe().f_back))()  # this is ok too
    #(lambda: pdb.Pdb().set_trace(sys._getframe().f_back.f_back))()  # this is not, again

    #print("hey")  # this line shows before the debugging starts
    #(lambda: print("hey"))()  # this line fixes _some_ things (and doesn't show up)

    # This above shows the reason the two lambda approaches and the call to
    # `pdb.set_trace` works; it puts a frame transition right after which lets
    # a chance for the debugging session to start. The direct `print` doesn't
    # cause a transition.


sys.breakpointhook = _db

hello = 1
breakpoint()
