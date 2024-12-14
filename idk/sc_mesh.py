#!/usr/bin/env python
from math import sin, cos, pi

class mesh:
    def __init__(self, mr= 6):
        assert not mr <3

        self.pts = [(0, 0, 0), (1, 0, 1.0/(mr+1))]
        self.inds = []
        self.at = 1

        for k in range(1, mr):
            self.inds.extend([0, k+1, k])
            a = (k * 2*pi)/mr
            self.pts.append((cos(a), sin(a), (k+1.0)/(mr+1)))

        self.inds.extend([
            0, 1, mr,
            len(self.pts), len(self.pts)-1, 1])
        self.pts.append((self.pts[1][0]+.4, self.pts[1][1]-.2, 1))

    def __str__(self):
        return "points:\n\t%s\nindices:\n\t%s" % (
            "\n\t".join(str(p) for p in self.pts),
            "\n\t".join(str(self.inds[k:k+3]) for k in range(0, len(self.inds), 3)))

    def sc(self):
        l = len(self.pts)
        self.inds.extend([
            l, l-1, self.at,
            l, self.at, self.at+1])

        a, b, c = self.pts[self.at:self.at+3]
        # the maths below are to position the new vertice slightly outward in
        # a ways that doing sc-only rings actually keeps the volume cylindrical
        # NOTE: the vertical direction is hard coded..

        mx, my = (a[0]+b[0])/2, (a[1]+b[1])/2
        xab, yab = b[0]-a[0], b[1]-a[1]
        xac, yac = c[0]-a[0], c[1]-a[1]

        dd = xab**2 + yab**2
        d = dd**.5
        nx, ny = yab/d, -xab/d

        sq = (xac*xab + yac*yab)**2  /  ((xac**2 + yac**2)*dd)
        tanacos = ((1-sq) / sq)**.5

        self.pts.append((
            mx + d/4*tanacos*nx,
            my + d/4*tanacos*ny,
            #(a[2]+b[2])/2 + d))
            (a[2]+b[2])/2 + 1))
        self.at+= 1

        return self

    def inc(self):
        a, b = self.pts[self.at:self.at+2]
        xab, yab = b[0]-a[0], b[1]-a[1]

        dd = xab**2 + yab**2
        d = dd**.5
        nx, ny = yab/d, -xab/d

        vix, viy = self.pts[-1][0], self.pts[-1][1]
        self.sc()
        ux, uy = self.pts[-1][0], self.pts[-1][1]

        rr = (ux-vix)**2 + (uy-viy)**2
        pox, poy = ux - xab/2, uy - yab/2

        iii = 2*(pox*nx + poy*ny - vix*nx - viy*ny)
        jjj = pox**2 + poy**2 + vix**2 + viy**2 - rr - 2*(pox*vix + poy*viy)
        sqrtdelta = (iii**2-4*jjj)**.5
        h1, h2 = abs(-iii+sqrtdelta), abs(-iii-sqrtdelta)
        h = min(h1, h2)/2

        self.pts[-1] = (
            pox + h*nx,
            poy + h*ny,
            self.pts[-1][2])

        self.at-= 1
        self.inds.pop(), self.inds.pop(), self.inds.pop()

        l = len(self.pts)
        self.inds.extend([
            l, l-1, self.at,
            l, self.at, self.at+1])
        self.pts.append((
            ux + xab/2 + h*nx,
            uy + yab/2 + h*ny,
            self.pts[-1][2]))
        self.at+= 1

        return self

    def dec(self):
        l = len(self.pts)
        self.inds.extend([
            l, l-1, self.at,
            l, self.at, self.at+1,
            l, self.at+1, self.at+2])

        a, b, c = self.pts[self.at:self.at+3]
        #d = ((b[0]-a[0])**2 + (b[1]-a[1])**2)**.5

        self.pts.append((
            (a[0]+c[0])/2,
            (a[1]+c[1])/2,
            #b[2] + d))
            b[2] + 1))
        self.at+= 2

        return self

    def count(self):
        return len(self.pts)-1 - self.at

    def fasten(self):
        l = len(self.pts)
        D = self.count()

        for k in range(1, D):
            self.inds.extend([
                l-k, l, l-k-1])
        self.inds.extend([
            l-D, l, l-1,
            l-D, l-1, self.at])
        self.at = len(self.pts)-1

        mx, my = 0, 0
        for px, py, _ in self.pts[- D: ]:
            mx+= px
            my+= py

        self.pts.append((
            mx/D,
            my/D,
            self.pts[-1][2]+1))

        return self

    def crepl(self, script=None):
        ask = (lambda _, it=iter(script.split('\n')): next(it)) if script else getattr(__builtins__, 'raw_input', input)

        def parseln(ln):
            cut = ln.find('#')
            com = (ln[:cut] if cut+1 else ln).strip(' \t\r\n\f0123456789')
            try: num = int(ln[:-len(com)])
            except: num = 1
            arg = com.split(None, 1)
            return (num, arg[0], arg[1] if len(arg)-1 else "") if arg else (0, "", "")

        def runcom(num, com, arg):
            if 'help' == com:
                if 'help' == arg: print("show help about a command")
                elif 'quit' == arg: print("quits")
                elif 'sc' == arg: print("n simple crochets")
                elif 'inc' == arg: print("n increases")
                elif 'dec' == arg: print("n decreases")
                elif 'rep' == arg: print("n repeats of a sequence of commands separated by ','")
                elif 'count' == arg: print("show the current stitch count")
                elif 'fasten' == arg: print("close and fasten off")
                else: print("commands: quit, help [<com>], [n]sc, [n]inc, [n]dec, [n]rep <coms>, count, fasten")

            elif 'sc' == com:
                for _ in range(num): self.sc()
            elif 'inc' == com:
                for _ in range(num): self.inc()
            elif 'dec' == com:
                for _ in range(num): self.dec()
            elif 'rep' == com:
                ls, depth = [""], 0
                for c in arg:
                    if depth:
                        ls[-1]+= c
                        depth+= ('(' == c) - (c == ')')
                    elif ',' == c: ls.append("")
                    else:
                        ls[-1]+= c
                        depth+= '(' == c # :)
                ls = [it.strip('()') for it in ls]
                for _ in range(num):
                    for it in ls:
                        n, c, a = parseln(it)
                        runcom(n, c, a)
            elif 'count' == com: print(self.count())
            elif 'fasten' == com: self.fasten()

            # (undocumented...)
            elif 'pyrender' == com:
                print(self.pyrender.__doc__)
                self.pyrender()

            else: print("unknown command: '" + com + "', type help for- well, help")

        while True:
            try:
                num, com, arg = parseln(ask("owo? "))
            except:
                break
            if not com: continue
            if 'quit' == com: break
            runcom(num, com, arg)

        return self

    # (dev tool; somewhat temporary)
    def pyrender(self, scene=None):
        """view key reminder:
        a: Toggles rotational animation mode.
        c: Toggles backface culling.
        f: Toggles fullscreen mode.
        h: Toggles shadow rendering.
        i: Toggles axis display mode.
        l: Toggles lighting mode.
        m: Toggles face normal visualization.
        n: Toggles vertex normal visualization.
        o: Toggles orthographic camera mode.
        q: Quits the viewer.
        r: Starts recording a GIF (press again to stop).
        s: Save the current view as an image.
        w: Toggles wireframe mode.
        z: Resets the camera to the default view.
        """
        import pyrender as pr
        import numpy as np
        scene = scene or pr.Scene()
        mesh = pr.Mesh(primitives= [pr.Primitive(
            positions= np.array(self.pts),
            indices= np.reshape(self.inds, (-1, 3)),
            mode= 4)]) # -> `TRIANGLES`
        node = scene.add(mesh)
        return mesh, node, pr.Viewer(scene, use_raymond_lighting= True, all_wireframe= True)

if '__main__' == __name__: mesh().crepl()
