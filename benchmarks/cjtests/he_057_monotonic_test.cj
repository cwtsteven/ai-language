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
    @Expect(typed_fn_solve(listOf([1, 2, 4, 10])), true)
}

@Test
func case1() {
    @Expect(typed_fn_solve(listOf([1, 2, 4, 20])), true)
}

@Test
func case2() {
    @Expect(typed_fn_solve(listOf([1, 20, 4, 10])), false)
}

@Test
func case3() {
    @Expect(typed_fn_solve(listOf([4, 1, 0, -10])), true)
}

@Test
func case4() {
    @Expect(typed_fn_solve(listOf([4, 1, 1, 0])), true)
}

@Test
func case5() {
    @Expect(typed_fn_solve(listOf([1, 2, 3, 2, 5, 60])), false)
}

@Test
func case6() {
    @Expect(typed_fn_solve(listOf([1, 2, 3, 4, 5, 60])), true)
}

@Test
func case7() {
    @Expect(typed_fn_solve(listOf([9, 9, 9, 9])), true)
}

@Test
func case8() {
    @Expect(typed_fn_solve(listOf([])), true)
}

@Test
func case9() {
    @Expect(typed_fn_solve(listOf([3])), true)
}

@Test
func case10() {
    @Expect(typed_fn_solve(listOf([1, 2])), true)
}

@Test
func case11() {
    @Expect(typed_fn_solve(listOf([2, 1])), true)
}

@Test
func case12() {
    @Expect(typed_fn_solve(listOf([5, 5, 4])), true)
}
