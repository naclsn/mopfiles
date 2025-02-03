from pyglet.math import Vec2, Vec3
import math
import pyglet

import os


def provider(id: object):
    path = str(id)
    try:
        return (os.path.join(path, it) for it in os.listdir(path))
    except BaseException as e:
        print(e)
        return []


ROOT = "./"


class Node:
    def __init__(self, id: object, x: float, y: float, /, app: "App"):
        self.id = id
        self.shape = pyglet.shapes.Circle(
            x,
            y,
            50,
            color=(55, 55, 255),
            batch=app.node_batch,
        )
        self.label = pyglet.text.Label(
            f"<{id}>",
            x,
            y,
            font_name="monospace",
            font_size=16,
            anchor_x="center",
            anchor_y="center",
            batch=app.node_batch,
        )
        self.lines: "list[pyglet.shapes.Line]" = []
        self.parents: "list[Node]" = []
        self.children: "list[Node]" = []
        self.pos = Vec2(x, y)
        self.velo = Vec2(0)

    def simulate(self, dt: float, nodes: "dict[int, Node]"):
        if not self.parents:
            self.pos = Vec2(0)

        else:
            force = Vec2(0)

            for other in self.parents:
                d = other.pos - self.pos
                force += d

            for other in nodes.values():
                if other is not self:
                    d = other.pos - self.pos
                    force -= 50 * d / d.length()

            self.velo += force  # * dt
            self.velo *= 0.8
            self.pos += self.velo  # * dt

        self.shape.position = self.pos
        self.label.x = self.pos.x
        self.label.y = self.pos.y

        # XXX: probably suck, updating some multiple times if refered to multiple times
        for other, line in zip(self.children, self.lines):
            other.simulate(dt, nodes)
            line.x, line.y = self.pos
            line.x2, line.y2 = other.pos


class Camera:
    def __init__(self, window: "App"):
        self._window = window
        self.zoom = 1.0
        self.offset_x = self.offset_y = 0.0

    def screen_to_world(self, x: float, y: float):
        tx = -self._window.width // 2 + self.offset_x * self.zoom
        ty = -self._window.height // 2 + self.offset_y * self.zoom

        return (x + tx) / self.zoom, (y + ty) / self.zoom

    def __enter__(self):
        tx = -self._window.width // 2 + self.offset_x * self.zoom
        ty = -self._window.height // 2 + self.offset_y * self.zoom

        view_matrix = self._window.view.translate(Vec3(-tx, -ty, 0))
        view_matrix = view_matrix.scale(Vec3(self.zoom, self.zoom, 1))
        self._window.view = view_matrix

    def __exit__(self, *_):
        tx = -self._window.width // 2 + self.offset_x * self.zoom
        ty = -self._window.height // 2 + self.offset_y * self.zoom

        view_matrix = self._window.view.scale(Vec3(1 / self.zoom, 1 / self.zoom, 1))
        view_matrix = view_matrix.translate(Vec3(tx, ty, 0))
        self._window.view = view_matrix


class App(pyglet.window.Window):
    def __init__(self, root: object):
        super().__init__(720, 480, resizable=True, caption="hi :3")
        self.time = 0
        self.camera = Camera(self)
        self.line_batch = pyglet.graphics.Batch()
        self.node_batch = pyglet.graphics.Batch()
        self.root = root
        self.nodes = {root: Node(root, 0, 0, self)}

    def on_draw(self):
        self.clear()
        with self.camera:
            self.line_batch.draw()
            self.node_batch.draw()

    def on_mouse_press(self, x, y, button, modifiers):
        self.press_at = self.time
        self.drag_dist = 0

    def on_mouse_release(self, x, y, button, modifiers):
        if 1 < self.time - self.press_at or 50 < self.drag_dist:
            return

        x, y = self.camera.screen_to_world(x, y)
        for r in self.nodes.values():
            if r.pos.distance(Vec2(x, y)) < 50:
                break
        else:
            return

        ls = list(provider(r.id))
        for k, cid in enumerate(ls):
            off = Vec2.from_heading(k / len(ls) * 2 * math.pi)

            n = self.nodes[cid] = Node(cid, x + off.x, y + off.y, self)
            n.parents.append(r)
            r.children.append(n)

            r.lines.append(
                pyglet.shapes.Line(
                    *r.pos,
                    x + off.x,
                    y + off.y,
                    thickness=12,
                    color=(255, 12, 123),
                    batch=self.line_batch,
                )
            )

    def on_mouse_drag(self, x, y, dx, dy, buttons, modifiers):
        self.drag_dist += dx * dx + dy * dy
        if 50 < self.drag_dist:
            self.camera.offset_x -= dx / self.camera.zoom
            self.camera.offset_y -= dy / self.camera.zoom

    def on_mouse_scroll(self, x, y, scroll_x, scroll_y):
        if scroll_y:
            self.camera.zoom *= 1.1 if 0 < scroll_y else 0.9

    def tick(self, dt: float):
        self.time += dt
        self.nodes[self.root].simulate(dt, self.nodes)


if __name__ == "__main__":
    app = App(ROOT)
    pyglet.clock.schedule_interval(app.tick, 1 / 24)
    pyglet.app.run()

# vim: se tw=88:
