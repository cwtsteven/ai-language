import std.collection.ArrayList
import std.unittest.*
import std.unittest.testmacro.*

func listOf(items: Array<Int64>): ArrayList<Int64> {
    let out = ArrayList<Int64>()
    for (item in items) {
        out.add(item)
    }
    return out
}

@Test
func case0() {
    @Expect(typed_fn_solve(2), listOf([2]))
}

@Test
func case1() {
    @Expect(typed_fn_solve(4), listOf([2, 2]))
}

@Test
func case2() {
    @Expect(typed_fn_solve(8), listOf([2, 2, 2]))
}

@Test
func case3() {
    @Expect(typed_fn_solve(57), listOf([3, 19]))
}

@Test
func case4() {
    @Expect(typed_fn_solve(3249), listOf([3, 3, 19, 19]))
}

@Test
func case5() {
    @Expect(typed_fn_solve(185193), listOf([3, 3, 3, 19, 19, 19]))
}

@Test
func case6() {
    @Expect(typed_fn_solve(20577), listOf([3, 19, 19, 19]))
}

@Test
func case7() {
    @Expect(typed_fn_solve(18), listOf([2, 3, 3]))
}

@Test
func case8() {
    @Expect(typed_fn_solve(3), listOf([3]))
}

@Test
func case9() {
    @Expect(typed_fn_solve(9), listOf([3, 3]))
}

@Test
func case10() {
    @Expect(typed_fn_solve(49), listOf([7, 7]))
}

@Test
func case11() {
    @Expect(typed_fn_solve(97), listOf([97]))
}

@Test
func case12() {
    @Expect(typed_fn_solve(100), listOf([2, 2, 5, 5]))
}
