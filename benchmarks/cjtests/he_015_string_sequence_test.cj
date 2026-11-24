import std.unittest.*
import std.unittest.testmacro.*

@Test
func case0() {
    @Expect(typed_fn_solve(0), "0")
}

@Test
func case1() {
    @Expect(typed_fn_solve(3), "0 1 2 3")
}

@Test
func case2() {
    @Expect(typed_fn_solve(10), "0 1 2 3 4 5 6 7 8 9 10")
}

@Test
func case3() {
    @Expect(typed_fn_solve(1), "0 1")
}

@Test
func case4() {
    @Expect(typed_fn_solve(2), "0 1 2")
}

@Test
func case5() {
    @Expect(typed_fn_solve(12), "0 1 2 3 4 5 6 7 8 9 10 11 12")
}
