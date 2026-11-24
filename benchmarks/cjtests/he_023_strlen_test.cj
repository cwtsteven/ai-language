import std.unittest.*
import std.unittest.testmacro.*

@Test
func case0() {
    @Expect(typed_fn_solve(""), 0)
}

@Test
func case1() {
    @Expect(typed_fn_solve("x"), 1)
}

@Test
func case2() {
    @Expect(typed_fn_solve("asdasnakj"), 9)
}

@Test
func case3() {
    @Expect(typed_fn_solve(" "), 1)
}

@Test
func case4() {
    @Expect(typed_fn_solve("ab cd"), 5)
}

@Test
func case5() {
    @Expect(typed_fn_solve("0123456789"), 10)
}
