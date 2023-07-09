vim9script
import './hien.vim'

def Init(): dict<any>
    return { width: 20, height: 20, fps: 0, keys: split('q') }
enddef

def Btnp(id: number, key: string): bool
    return 'q' == key
enddef

def Loop(id: number): bool
    return false
enddef

hien.Register('Minesweeper', Init, Btnp, Loop)
