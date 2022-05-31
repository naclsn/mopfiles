class Node:
    _lastId = 0

    def __init__(self, source: str, sa: int, so: int):
        self.before: Node = None
        self.after: Node = None
        self.under: Chain = None
        self.over: Chain = None

        # self.past = past
        # self.future = future

        self.source = source
        self.sa = sa
        self.so = so

        self._id = Node._lastId
        Node._lastId+= 1

    def __len__(self):
        return self.so - self.sa

    def __str__(self):
        return self.source[self.sa:self.so]

    def __repr__(self):
        return f"{self._id: 3}: '{self.source}'[{self.sa}:{self.so}] (<{self.before and self.before._id} >{self.after and self.after._id} v{repr(self.under)} ^{repr(self.over)})"

    def __bool__(self): # override use of __len__
        return True

class Chain:
    _lastId = 0

    def __init__(self, san: Node, son: Node):
        self.san = san
        self.son = son

        self._id = Chain._lastId
        Chain._lastId+= 1

    def __repr__(self):
        return f"[{self._id}]{self.san and self.san._id},{self.son and self.son._id}"

class Place:
    def __init__(self, node: Node, index: int):
        self.node = node
        self.index = index # index \in [node.sa, node.so] or something

class Range:
    def __init__(self, a, b, c, d): # TODO: names and much more
        """
        a range is a quadruple of places as follow:
                a   b       c   d
                |   |-------|   |
            x =   (   6 * 7   )
        """
        pass

actual_stack: 'list[Node]' = []

def link2(a: Node, b: Node):
    """ links a <-> b; either may be null, will link to None if so """
    if a: a.after = b
    if b: b.before = a

def link3(a: Node, b: Node, c: Node):
    """ links a <-> b <-> c; same as a 2 link2 """
    link2(a, b)
    link2(b, c)

class Mesh:
    def __init__(self, source: str):
        self.handle = self.hondle = Node(source, 0, len(source))
        actual_stack.append(self.handle)

    def __len__(self):
        return len(str(self))

    def __str__(self):
        r, it = "", self.handle
        while it: r, it = r + str(it), it.after
        return r

    def __repr__(self):
        r, it = "", self.handle
        while it: r, it = r + f"\t{repr(it)}\n", it.after
        return r[:-1] # remove last newline

    def _find_containing(self, ordered: 'list[int]') -> 'list[tuple[Node, int]]':
        """
        TODO: remove asap in favor of using `Place`s

        returns visible node such as `sa < k <= so`
        for `k` in `sorted` as a "global" index (rope level),

        this also mean `0 < k <= self.len`

        `ordered` needs to be sorted from lowest to highest index
        """
        r = []

        count = len(ordered)
        curr = 0
        k = ordered[0]
        acc = 0

        it = self.handle
        while it:
            l = len(it)
            while acc < k <= acc + l:
                r.append((it, acc))
                curr+= 1
                if curr == count: break
                k = ordered[curr]
            else:
                acc+= l
                it = it.after
                continue
            break

        assert len(r) == len(ordered), f"in _find_containing: lengths differ for {r} and {ordered}"
        return r

    def _find_crap(self, sa: int, so: int) -> 'tuple[Node, int, Node, int]':
        """
        TODO: same as _find_containing
        used to turn sa/so into san/son and their lsa/lso
        """
        if 0 == so:
           san = son = (self.handle, 0)
        elif 0 == sa:
           san = (self.handle, 0)
           son,= self._find_containing([so])
        else:
           san, son = self._find_containing([sa, so])

        lsa, san = sa + san[0].sa-san[1], san[0]
        lso, son = so + son[0].sa-son[1], son[0]

        return san, lsa, son, lso

    def _find_undoable_in(self, san: Node, son: Node) -> Node:
        """
        finds, from san to son included, the most recently
        added node that can be target of an undo operation

        san to son forms a linked list when nodes are in order of appearance
        actual_stack contains the node ordered by addition (recent appended last)

        an 'undoable' node is a node that has a non-null .under
        """
        node, best = None, 0
        it = san
        while it is not son:
            curr = actual_stack.index(it)
            if it.under and best < curr:
                node, best = it, curr
            it = it.after
        # unrolled last iteration
        curr = actual_stack.index(it)
        if it.under and best < curr:
            node, best = it, curr
        it = it.after
        return node

    def _find_redoable_in(self, san: Node, son: Node) -> Node:
        """
        finds, from san to son included, the least recently
        added node that can be target of an redo operation

        san to son forms a linked list when nodes are in order of appearance
        actual_stack contains the node ordered by addition (recent appended last)

        an 'redoable' node is a node that has a non-null .over
        """
        node, best = None, len(actual_stack)
        it = san
        while it is not son:
            curr = actual_stack.index(it)
            if it.over and curr < best:
                node, best = it, curr
            it = it.after
        curr = actual_stack.index(it)
        if it.over and curr < best:
            node, best = it, curr
        it = it.after
        return node

    def edit(self, sa: int, so: int, text: str):
        """```
           |------san------|   <...>   |------son------| # existing
                   |------------niw------------|         # to be added

        +san.sa         +san.so     +son.sa           +son.so
           |--xan--:--san--|   <...>   |--son--:--xon--|
                   |------------niw------------|
                 +lsa                        +lso

           |--xan--|------------niw------------|--xon--| # visible chain
                      v ^^               ^^ v
                   |--son--|   <...>   |--san--|         # "hidden chain"
        ```"""
        # assert 0 <= sa <= so <= self.length, "in edit: erroneous range" # cant be bother maintainig length for now

        # find nodes
        san, lsa, son, lso = self._find_crap(sa, so)

        niw = Node(text, 0, len(text))

        # perform the node splitting
        xan = Node(san.source, san.sa, lsa)
        san.sa = lsa
        xon = Node(son.source, lso, son.so)
        son.so = lso

        actual_stack.append(xan)
        actual_stack.append(xon)

        # update the under/over subchains
        #   o1 <...> o2
        #     ^v   v^
        #       san
        #     v^   ^v
        #   u1 <...> u2
        xan.over = san.over
        xan.under = san.under
        if xan.over and xan.over.san.under: xan.over.san.under.san = xan
        if xan.under and xan.under.san.over: xan.under.san.over.san = xan
        # at this point:
        #   o1 <...> o2
        #    ^v  X  v^
        #   xan <-> san
        #    v^  X  ^v
        #   u1 <...> u2
        xon.over = son.over
        xon.under = son.under
        if xon.over and xon.over.son.under: xon.over.son.under.son = xon
        if xon.under and xon.under.son.over: xon.under.son.over.son = xon
        # (same with son <-> xon, but xon is a subchain stop end)

        # insert the new nodes on either sides of the to-be-hidden chain
        xxa = san.before
        link3(xxa, xan, san)

        xxo = son.after
        link3(son, xon, xxo)
        # at this point:
        #   xxa <-> xan <-> san <...> son <-> xon <-> xxo
        #               ^-- to be hidden ---^

        link3(xan, niw, xon)

        hidden = Chain(san, son)
        visible = Chain(niw, niw)
        niw.under = hidden
        san.over = visible
        son.over = visible
        # at this point:
        #   xxa <-> xan <-> niw <-> xon <-> xxo
        #                 v^^ ^^v
        #               san<...>son

        # XXX: for now chains are stored on the stack
        actual_stack.append(hidden)
        actual_stack.append(visible)

        if san is self.handle: self.handle = xan
        if son is self.hondle: self.hondle = xon
        actual_stack.append(niw)

    def undo(self, sa, so):
        # assert 0 <= sa <= so <= self.length, "in undo: erroneous range" # cant be bother maintainig length for now

        # find nodes
        san, lsa, son, lso = self._find_crap(sa, so)
        # YYY: should use lsa/lso?

        # find the two ends of a visible chain to undo (replace with what is under it)
        found = self._find_undoable_in(san, son)
        # `found` is either end (whichever)
        # `found.under` is the hidden chain to be made visible
        oad = found.under.san
        ood = found.under.son
        # `oad.over` (= `ood.over`) is the currently visible chain
        # note that at least one of `naw` or `now` will be `found`
        naw = oad.over.san
        now = oad.over.son

        xan = naw.before
        xon = now.after
        # at this point:
        #   xan <-> naw <...> now <-> xon
        link2(xan, oad)
        link2(ood, xon)
        # at this point:
        #   xan <-> oad <...> ood <-> xon

    def redo(self, sa, so):
        # assert 0 <= sa <= so <= self.length, "in undo: erroneous range" # cant be bother maintainig length for now

        # find nodes
        san, lsa, son, lso = self._find_crap(sa, so)
        # YYY: should use lsa/lso?

        # find the two ends of a visible chain to redo (replace with what is over it)
        found = self._find_redoable_in(san, son)
        # `found` is either end (whichever)
        # `found.over` is the hidden chain to be made visible
        naw = found.over.san
        now = found.over.son
        # `naw.under` (= `now.under`) is the currently visible chain
        # note that at least one of `oad` or `ood` will be `found`
        oad = naw.under.san
        ood = naw.under.son

        xan = oad.before
        xon = ood.after
        # at this point:
        #   xan <-> oad <...> ood <-> xon
        link2(xan, naw)
        link2(now, xon)
        # at this point:
        #   xan <-> naw <...> now <-> xon

if '__main__' == __name__:
    a = None
    def print_state():
        print(a, "\n"+repr(a))
        if actual_stack: print("\n".join(("> " if it is a.handle else "< " if it is a.hondle else "  ") + repr(it) for it in actual_stack))
        print()

    a = Mesh("Hello world!")
    print_state();  print('\t\t\ta.edit(5, 6, " potato ")');  a.edit(5, 6, " potato ")
    print_state();  print('\t\t\ta.edit(9, 19, "o dayo")');   a.edit(9, 19, "o dayo")
    print_state();  print('\t\t\ta.undo(0, len(a))');         a.undo(0, len(a))
    print_state();  print('\t\t\ta.undo(0, len(a))');         a.undo(0, len(a))
    print_state();  print('\t\t\ta.redo(0, len(a))');         a.redo(0, len(a))
    print_state();  print('\t\t\ta.edit(6, 6, "my ")');       a.edit(6, 6, "my ")
    print_state();  print('\t\t\ta.edit(9, 16, "")');         a.edit(9, 16, "")
    print_state()
