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
    @Expect(typed_fn_solve(listOf([5])), true)
}

@Test
func case1() {
    @Expect(typed_fn_solve(listOf([1, 2, 3, 4, 5])), true)
}

@Test
func case2() {
    @Expect(typed_fn_solve(listOf([1, 3, 2, 4, 5])), false)
}

@Test
func case3() {
    @Expect(typed_fn_solve(listOf([1, 2, 3, 4, 5, 6])), true)
}

@Test
func case4() {
    @Expect(typed_fn_solve(listOf([1, 2, 3, 4, 5, 6, 7])), true)
}

@Test
func case5() {
    @Expect(typed_fn_solve(listOf([1, 3, 2, 4, 5, 6, 7])), false)
}

@Test
func case6() {
    @Expect(typed_fn_solve(listOf([])), true)
}

@Test
func case7() {
    @Expect(typed_fn_solve(listOf([1])), true)
}

@Test
func case8() {
    @Expect(typed_fn_solve(listOf([3, 2, 1])), false)
}

@Test
func case9() {
    @Expect(typed_fn_solve(listOf([1, 2, 2, 2, 3, 4])), false)
}

@Test
func case10() {
    @Expect(typed_fn_solve(listOf([1, 2, 3, 3, 3, 4])), false)
}

@Test
func case11() {
    @Expect(typed_fn_solve(listOf([1, 2, 2, 3, 3, 4])), true)
}

@Test
func case12() {
    @Expect(typed_fn_solve(listOf([1, 2, 3, 4])), true)
}

@Test
func case13() {
    @Expect(typed_fn_solve(listOf([1, 1])), true)
}

@Test
func case14() {
    @Expect(typed_fn_solve(listOf([1, 1, 1])), false)
}

@Test
func case15() {
    @Expect(typed_fn_solve(listOf([2, 2, 3])), true)
}

@Test
func case16() {
    @Expect(typed_fn_solve(listOf([-1, 0, 0])), true)
}
