#!/usr/bin/python3
#./__init__.py
#__all__ = [names, stack, build]

def usage():
	return """interpreter for fold; expects
REPL commands:
 - see: print the whole stack
 - pop: pop the top of the stack
 - peek: peek the top of the stack
 - clear: clear the whole stack
 - count: print the size of the stack
 - help: print this help
 - quit: quit the REPL
fold sources:
 - atoms: any of word, string, number or one of:
 - '(' word atoms ')': structure
 - '{' word atoms '}': evaluation
 - '[' word atoms ']': assignment"""

names = {}
stack = []
frames = []

OP_FRM="frame" # operand: fn pointer
OP_VAL="value" # operand: value
OP_FLD="fold"  # operand: arg number

class Instr:
	def __init__(self, kind, operand):
		self.kind = kind
		self.operand = operand
	def __str__(self):
		return "%s_%s(%s)" % (self.kind, type(self.operand).__name__, str(self.operand))
	def __repr__(self):
		return str(self)
class word:
	def __init__(self, name): self.name = name
	def __str__(self): return self.name
	def __repr__(self): return self.name

def eval(a):
	global names
	global stack

	stack, instrs = stack[:a], stack[a:]
	for it in instrs:
		if OP_FRM == it.kind:
			stack.append(it.operand)
		elif OP_FLD == it.kind:
			stack, fold = stack[:-1-it.operand], stack[-1-it.operand:]
			stack.append(call(fold[0], fold[1:]))
		elif OP_VAL == it.kind:
			stack.append(it.operand)

	return stack and stack[-1] or None
#end dev eval

def build(c):
	global stack
	global frames # [.., address, intent, word(?), arity, ..]
	SPECIAL = "({[)}]"
	NOT_WORD = " \t\r\n" + SPECIAL

	k = 0
	while k < len(c):
		while c[k] not in SPECIAL and c[k] in NOT_WORD:
			k+= 1
			if len(c) == k: return stack

		#  structure      evaluation     assignment
		if '(' == c[k] or '{' == c[k] or '[' == c[k]:
			z = c[k]
			k+= 1
			p = k
			# expects word
			while c[k:k+1] not in NOT_WORD: k+= 1
			stack.append(Instr(OP_FRM, c[p:k]))
			# not if first frame
			if frames: frames[-1]+= 1
			# address, intent, word, arity
			frames.append(len(stack)-1)
			frames.append(z)
			frames.append(c[p:k])
			frames.append(0)
			continue

		#  structure      evaluation     assignment
		if ')' == c[k] or '}' == c[k] or ']' == c[k]:
			z = c[k]
			k+= 1
			# arity, word, intent, address
			p = frames.pop()
			n = frames.pop()
			i = frames.pop()
			a = frames.pop()
			# check for syntax error
			if SPECIAL[SPECIAL.index(i)+3] != z: raise SyntaxError("unbalanced pair: open '%s' - close '%s'" % (i, z))
			stack.append(Instr(OP_FLD, p))
			# evaluation
			if '}' == z:
				stack.append(Instr(OP_VAL, eval(a)))
			# assignment
			if ']' == z:
				# 'fold' instruction
				if 1 != stack.pop().operand: raise SyntaxError("complex assignment not supported yet (if ever): assigning to '%s'" % n)
				# assigned value
				stack, names[n] = stack[:a+1], stack[a+1:]
				# 'frame' instruction
				stack.pop()
				stack.append(Instr(OP_VAL, word(n)))
			continue

		# other atoms (string, number, word)
		p = k
		# not if first frame
		if frames: frames[-1]+= 1
		if '"' == c[k]:
			k+= 1
			while c[k:k+1] not in '"': k+= 1
			k+= 1
			stack.append(Instr(OP_VAL, str(c[p+1:k-1])))
		else:
			while c[k:k+1] not in NOT_WORD: k+= 1
			try:
				stack.append(Instr(OP_VAL, int(c[p:k])))
			except ValueError:
				stack.append(Instr(OP_VAL, word(c[p:k])))
	#end while c[k++]

	return stack
#end def build

def call(f, a):
	return (f if callable(f) else names[str(f)])(a)
def apply(f, a):
	return list(map(f if callable(f) else names[str(f)], a))
def fold(f, i, a):
	if not callable(f): names[str(f)]
	for it in a: i = f(i, it)
	return i
names.update({
	'': lambda a: call(a[0], a[1:]) if a else None,
	',': lambda a: a,
	'@': lambda a: apply(a[0], a[1:]),
	'#': lambda a: fold(a[0], a[1], a[2:]),
	'+': lambda a: sum(a),
	'-': lambda a: a[0] - sum(a[1:]),
	'*': lambda a: fold(lambda it, ac: ac*it, 1, a),
	'/': lambda a: a[0] / fold(lambda it, ac: ac*it, 1, a),
	'out': lambda a: print(*a),
	'in': lambda a: input(a),
	'int': lambda a: apply(int, a),
	'str': lambda a: apply(str, a),
})

if '__main__' == __name__:
	from sys import argv, stderr
	if argv[1:]:
		if '-h' == argv[1]:
			print("Usage:", argv[0], "[-h | -l | <file>]", file=stderr)
			print(usage(), file=stderr)
			exit(2)
		if '-l' == argv[1]:
			print("Builtin names:", file=stderr)
			print(list(names.keys()), file=stderr)
			exit(2)

		if '--' == argv[1]: argv.pop(1)
		# try with file content
		try:
			with open(argv[1]) as f:
				print(build(f.read()))
				print(names)
		except BaseException as err:
			print(err)
			exit(1)
		exit(0)

	# quick hack; balanced does not imply valid
	def is_balanced(s):
		counts = { '(': 0, ')': 0, '{': 0, '}': 0, '[': 0, ']': 0 }
		in_str = False
		for it in s:
			if '"' == it: in_str = not in_str
			elif not in_str and it in counts.keys(): counts[it]+= 1
		return counts['('] == counts[')'] \
		   and counts['{'] == counts['}'] \
		   and counts['['] == counts[']']

	while True:
		try:
			user = input("!! ")
			while not is_balanced(user): user+= input(".. ")
			user = user.strip()
			# REPL commands
			if   "see"   == user: print(stack)
			elif "pop"   == user: print(stack.pop())
			elif "peek"  == user: print(stack[-1])
			elif "clear" == user: print(stack.clear())
			elif "count" == user: print(len(stack))
			elif "help"  == user: print(usage())
			elif "quit"  == user: exit(0)
			# actual sources
			else: print(build(user))
		except (EOFError, KeyboardInterrupt):
			break
		except BaseException as err:
			print(err)
	print()
