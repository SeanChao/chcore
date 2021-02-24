set architecture aarch64
target remote localhost:1234
file ./build/kernel.img

define nir
    ni
    p/x $x29
    p/x $sp
end

define showstack
    p/x $x29
    p/x $sp
end

# break stack_test
break stack_backtrace
