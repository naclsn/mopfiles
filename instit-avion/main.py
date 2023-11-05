#!/bin/env python
if 'raw_input' in dir(__builtins__): input = raw_input

try:
    dico = {}
    with open(__file__[:-len('main.py')] + 'cmudict-0.7b', 'r') as f:
        for ln in f.readlines():
            if ln[0] in '0123456789ABCDEFGHIJKLMNOPQRSTUVWXZY\'':
                word, phonsl = ln.strip().split('  ', 1)
                if word[-1] == ')': word = word[:-3]
                if word not in dico: dico[word] = []
                phonss = [it[:-1] if it[-1] in '012' else it for it in phonsl.split()]
                if phonss not in dico[word]: dico[word].append(phonss)
except IOError:
    print("could not find cmudict-0.7b, get it here:")
    print("https://raw.githubusercontent.com/Alexir/CMUdict/master/cmudict-0.7b")
    exit(1)

def glob(pat, phons):
    s = 0
    for it in phons:
        if len(pat) == s: return False
        if '?' == pat[s] or it == pat[s] or list == type(pat[s]) and it in pat[s]:
            s+= 1
        elif '*' == pat[s]:
            if len(pat) == s+1: return True
            elif it == pat[s+1]: s+= 2
        else: return False
    return len(pat) == s

class coms:
    @staticmethod
    def phon(arg):
        """<words> print the phonetic transcription of a sentence"""
        r = []
        for w in arg.split():
            r.append((w, dico.get(w.strip('.,;()!?-"').upper(), [["(?)"]])))
        return r

    @staticmethod
    def find(arg):
        """<phons> search for a word with the pattern of sounds"""
        r = []
        pat = arg.split()
        for k in range(len(pat)):
            if '{' == pat[k][0] and '}' == pat[k][-1]:
                pat[k] = filter(None, pat[k][1:-1].split(','))
        for w, phonss in dico.items():
            if any(glob(pat, phons) for phons in phonss):
                r.append((w.lower(), phonss))
        return r

    @staticmethod
    def asin(arg):
        """<phons> reminder on transcription (~ARPAbet)"""
        asin = {'AA': ("odd", [["AA", "D"]]), 'AE': ("at", [["AE", "T"]]), 'AH': ("hut", [["HH", "AH", "T"]]), 'AO': ("ought", [["AO", "T"]]), 'AW': ("cow", [["K", "AW"]]), 'AY': ("hide", [["HH", "AY", "D"]]), 'B': ("be", [["B", "IY"]]), 'CH': ("cheese", [["CH", "IY", "Z"]]), 'D': ("dee", [["D", "IY"]]), 'DH': ("thee", [["DH", "IY"]]), 'EH': ("Ed", [["EH", "D"]]), 'ER': ("hurt", [["HH", "ER", "T"]]), 'EY': ("ate", [["EY", "T"]]), 'F': ("fee", [["F", "IY"]]), 'G': ("green", [["G", "R", "IY", "N"]]), 'HH': ("he", [["HH", "IY"]]), 'IH': ("it", [["IH", "T"]]), 'IY': ("eat", [["IY", "T"]]), 'JH': ("gee", [["JH", "IY"]]), 'K': ("key", [["K", "IY"]]), 'L': ("lee", [["L", "IY"]]), 'M': ("me", [["M", "IY"]]), 'N': ("knee", [["N", "IY"]]), 'NG': ("ping", [["P", "IH", "NG"]]), 'OW': ("oat", [["OW", "T"]]), 'OY': ("toy", [["T", "OY"]]), 'P': ("pee", [["P", "IY"]]), 'R': ("read", [["R", "IY", "D"]]), 'S': ("sea", [["S", "IY"]]), 'SH': ("she", [["SH", "IY"]]), 'T': ("tea", [["T", "IY"]]), 'TH': ("theta", [["TH", "EY", "T", "AH"]]), 'UH': ("hood", [["HH", "UH", "D"]]), 'UW': ("two", [["T", "UW"]]), 'V': ("vee", [["V", "IY"]]), 'W': ("we", [["W", "IY"]]), 'Y': ("yield", [["Y", "IY", "L", "D"]]), 'Z': ("zee", [["Z", "IY"]]), 'ZH': ("seizure", [["S", "IY", "ZH", "ER"]])}
        ls = com[4:].split()
        if not ls: return list(asin.values())
        r = []
        for p in ls: r.append(asin.get(p, (p, [["(?)"]])))
        return r

if '__main__' == __name__:
    while True:
        try: com = input("> ")
        except: break

        if hasattr(coms, com[:4]):
            r = getattr(coms, com[:4])(com[4:])
            if list == type(r):
                for word, phonss in r: print(word + ": " + "; ".join(" ".join(it) for it in phonss))
            else: print(r)

        elif 'quit' == com[:4]:
            break
        elif 'help' == com[:4]:
            for com in dir(coms):
                if not com.startswith('__'): print(com + " " + getattr(coms, com).__doc__)
