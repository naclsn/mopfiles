from pyglet.math import Vec2, Vec3
from pyglet.window import key
import math
import pyglet

from typing import TYPE_CHECKING
from collections.abc import Callable, Iterable

if TYPE_CHECKING:
    from typing import TypeVar, override

    Ref_ = TypeVar("Ref_")
    Provider_ = Callable[[Ref_], Iterable[Ref_]]
else:
    override = lambda m, /: m

# {{{ test providers
import os


def fs_provider(ref: str):
    path = ref
    try:
        return (os.path.join(path, it) for it in os.listdir(path))
    except BaseException as e:
        print(e)
        return list[str]()


fs_provider.ROOT = "./"
# +++


class object_ref:
    _bag: 'dict[int, object_ref]' = {}

    def __new__(cls, key: "str | int", object: object):
        return object_ref._bag.setdefault(id(object), super().__new__(cls))

    def __init__(self, key: "str | int", object: object):
        self.key = key
        self.object = object

    @override
    def __str__(self):
        return f"{self.key} -- {self.object!r} ({id(self.object)})"

def ref_provider(ref: object_ref):
    try:
        yield from (object_ref(k, e) for k, e in enumerate(ref.object))
    except:
        pass
    try:
        yield from (object_ref(k, e) for k, e in enumerate(vars(ref.object)))
    except:
        pass
    yield object_ref("__class__", type(ref.object))


ref_provider.ROOT = object_ref("_", ['a', 'b', 'c'])
# }}}


class CameraGroup(pyglet.graphics.Group):
    def __init__(
        self,
        x: int,
        y: int,
        width: int,
        height: int,
        order: int = 0,
        parent: "pyglet.graphics.Group | None" = None,
    ):
        super().__init__(order, parent)
        self.x, self.y = x, y
        self.width, self.height = width, height
        self.offset_x = self.offset_y = 0.0
        self.zoom = 1.0

    @property
    def area(self):
        return self.x, self.y, self.width, self.height

    @area.setter
    def area(self, area: "tuple[int, int, int, int]"):
        self.x, self.y, self.width, self.height = area

    @override
    def set_state(self):
        # no `pyglet.gl.glPushMatrix` & co anymore, so here's the hack around
        win = next(win for win in pyglet.app.windows if win.context is pyglet.gl.current_context)
        assert isinstance(win, pyglet.window.Window)
        self.win = win
        self.pview = win.view

        pyglet.gl.glEnable(pyglet.gl.GL_SCISSOR_TEST)
        pyglet.gl.glScissor(self.x, self.y, self.width, self.height)

        tx = -self.width // 2 + self.offset_x * self.zoom
        ty = -self.height // 2 + self.offset_y * self.zoom
        win.view = win.view.translate(Vec3(-tx, -ty, 0)).scale(Vec3(self.zoom, self.zoom, 1))

    @override
    def unset_state(self):
        pyglet.gl.glDisable(pyglet.gl.GL_SCISSOR_TEST)

        self.win.view = self.pview
        del self.win
        del self.pview

    def screen_to_view(self, x: float, y: float):
        tx = -self.width // 2 + self.offset_x * self.zoom
        ty = -self.height // 2 + self.offset_y * self.zoom

        return (x + tx) / self.zoom, (y + ty) / self.zoom

    def view_to_screen(self, x: float, y: float):
        assert not "implemented"
        return x, y


class Node:
    def __init__(
        self,
        ref: "Ref_",
        x: float,
        y: float,
        batch: "pyglet.graphics.Batch | None" = None,
        group: "pyglet.graphics.Group | None" = None,
    ):
        self.ref = ref
        self.shape = pyglet.shapes.Circle(
            x,
            y,
            50,
            color=(55, 55, 255),
            batch=batch,
            group=group,
        )
        self.label = pyglet.text.Label(
            str(ref),
            x,
            y,
            font_name="monospace",
            font_size=16,
            anchor_x="center",
            anchor_y="center",
            batch=batch,
            group=group,
        )
        self.lines: "list[pyglet.shapes.Line]" = []
        self.parents: "set[Node]" = set()
        self.children: "set[Node]" = set()
        self.pos = Vec2(x, y)
        self.velo = Vec2(0)

    def _attraction(self: "Node", other: "Node"):
        d = other.pos - self.pos
        l = d.length()
        return d * l / 10000

    def _repulsion(self: "Node", other: "Node"):
        d = other.pos - self.pos
        l = d.length()
        if l < 0.0001:
            return 0
        n = d / l
        return 1000 * n / l

    def simulate(self):
        if not self.parents:
            self.pos = Vec2(0)

        else:
            force = Vec2(0)

            for parent in self.parents:
                force += Node._attraction(self, parent)

                for sibling in parent.children:
                    if sibling is not self:
                        force -= Node._repulsion(self, sibling)
                    force -= Node._repulsion(self, parent) * (len(self.children) + 1) * 0.8

                    for relative in sibling.children:
                        if relative is not self:
                            force -= Node._repulsion(self, relative) * 0.5

                for grandparent in parent.parents:
                    force -= Node._repulsion(self, grandparent)

            self.velo += force  # * dt
            self.velo *= 0.8
            self.pos += self.velo  # * dt

        self.shape.position = self.pos
        self.label.x = self.pos.x
        self.label.y = self.pos.y

        for other, line in zip(self.children, self.lines):
            line.x, line.y = self.pos
            # xxx: incorrect because child will be updated later on, but meh
            line.x2, line.y2 = other.pos


class Graph(pyglet.gui.WidgetBase):
    def __init__(
        self,
        x: int,
        y: int,
        width: int,
        height: int,
        root: "Ref_",
        provider: "Provider_[Ref_]",
        batch: "pyglet.graphics.Batch | None" = None,
        group: "pyglet.graphics.Group | None" = None,
    ):
        super().__init__(x, y, width, height)

        self.time = 0
        self.camera = CameraGroup(x, y, width, height, parent=group)
        self.drag_dist = 0

        self._user_batch = batch
        self._user_group = group
        self.node_group = pyglet.graphics.Group(order=1, parent=self.camera)
        self.line_group = pyglet.graphics.Group(order=0, parent=self.camera)

        self.root = root
        self.provider = provider
        self.nodes = {root: Node(root, 0, 0, batch=batch, group=self.node_group)}

        self._set_enabled(self._enabled)

    @override
    def _set_enabled(self, enabled: bool):
        if enabled:
            pyglet.clock.schedule_interval(self.tick, 1 / 24)
        else:
            pyglet.clock.unschedule(self.tick)

    def update_size(self, width: int, height: int):
        self.camera.width = self._width = width
        self.camera.height = self._height = height

    @override
    def _update_position(self):
        self.camera.area = self.x, self.y, self.width, self.height

    # {{{ events
    # TODO(multiple below): self._check_hit, and is there a concept of focused?

    @override
    def on_key_press(self, symbol: int, modifiers: int):
        match symbol, modifiers:
            # XXX: temporary
            case key.SPACE, _:
                self.unfold(self.nodes[self.root])

            case (key.PAGEUP, _) | (key.PLUS, key.MOD_CTRL):
                self.camera.zoom *= 1.4
            case (key.PAGEDOWN, _) | (key.MINUS, key.MOD_CTRL):
                self.camera.zoom *= 0.6
            case (key.HOME, _) | (key.EQUAL, key.MOD_CTRL):
                self.camera.zoom = 1
            case key.HOME, key.MOD_CTRL:
                self.camera.offset_x = 0
                self.camera.offset_y = 0
                self.camera.zoom = 1

            case _:
                pass

    @override
    def on_mouse_press(self, x: int, y: int, buttons: int, modifiers: int):
        self.press_at = self.time
        self.drag_dist = 0

    @override
    def on_mouse_release(self, x: int, y: int, buttons: int, modifiers: int):
        if 1 < self.time - self.press_at or 50 < self.drag_dist:
            return

        wor = Vec2(*self.camera.screen_to_view(x, y))
        for node in self.nodes.values():
            if node.pos.distance(wor) < 50:
                break
        else:
            return

        self.unfold(node)

    @override
    def on_mouse_drag(self, x: int, y: int, dx: int, dy: int, buttons: int, modifiers: int):
        self.drag_dist += dx * dx + dy * dy
        if 50 < self.drag_dist:
            self.camera.offset_x -= dx / self.camera.zoom
            self.camera.offset_y -= dy / self.camera.zoom

    @override
    def on_mouse_scroll(self, x: int, y: int, scroll_x: float, scroll_y: float):
        if scroll_y:
            self.camera.zoom *= 1.1 if 0 < scroll_y else 0.9

    # }}}

    def focus(self, node: Node):
        assert not "implemented"

    def unfold(self, node: Node):
        ls = list(self.provider(node.ref))
        for k, ref in enumerate(ls):
            off = Vec2.from_heading(k / len(ls) * 2 * math.pi, 50)

            if ref not in self.nodes:
                self.nodes[ref] = Node(
                    ref,
                    *(node.pos + off),
                    batch=self._user_batch,
                    group=self.node_group,
                )
            child = self.nodes[ref]
            child.parents.add(node)
            node.children.add(child)

            line = pyglet.shapes.Line(
                *node.pos,
                *child.pos,
                thickness=12,
                color=(255, 12, 123),
                batch=self._user_batch,
                group=self.line_group,
            )
            node.lines.append(line)

    def tick(self, dt: float):
        self.time += dt
        # rem: works because insertion order
        for n in self.nodes.values():
            n.simulate()


class App(pyglet.window.Window):
    def __init__(self, root: "Ref_", provider: "Provider_[Ref_]"):
        super().__init__(720, 480, resizable=True, caption="hi :3")
        self.batch = pyglet.graphics.Batch()
        self.graph = Graph(0, 0, 720, 480, root, provider, batch=self.batch)
        self.push_handlers(self.graph)

    @override
    def on_draw(self):
        self.clear()
        self.batch.draw()

    @override
    def on_resize(self, width: int, height: int):
        self.graph.update_size(width, height)


if __name__ == "__main__":
    p = ref_provider
    app = App(p.ROOT, p)
    pyglet.app.run()

# vim: se tw=99:
