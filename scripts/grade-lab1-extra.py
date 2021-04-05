from gradelib import *

r = Runner(save("chcore.out"),
           stop_breakpoint("break_point"))


@test(0, "running ChCore")
def test_chcore():
    r.run_qemu(30)


@test(10, "print hex")
def test_print_hex():
    r.match('\[INFO\] 23333 dec -> 0x5b25 hex')


@test(10, "print dec")
def test_print_dec():
    r.match('\[INFO\] 23333 dec -> 23333 dec')


@test(30, "print padding")
def test_print_padding():
    r.match('\[INFO\] padding left1:    42')
    r.match('\[INFO\] padding left2: 000000000666')
    r.match('\[INFO\] padding right: 1  \$')


@test(60, "print misc")
def test_print_misc():
    r.match('\[INFO\] unsigned: 4294967295')
    r.match('\[INFO\] pointer: 0xdeadbeef')
    r.match('\[INFO\] char: x')
    r.match('\[INFO\] string: wubba lubba dub dub')
    r.match('\[INFO\] long: -1')
    r.match('\[INFO\] short short: 97')


@test(40, "print neg")
def test_print_neg():
    r.match('\[INFO\] neg 1: -1')
    r.match('\[INFO\] neg INT_MIN: -2147483648')
    r.match('\[INFO\] neg pad -02')
    r.match('\[INFO\] neg pad right -1  #')


run_tests()
