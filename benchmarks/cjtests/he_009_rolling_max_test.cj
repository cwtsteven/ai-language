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
    @Expect(typed_fn_solve(listOf([])), listOf([]))
}

@Test
func case1() {
    @Expect(typed_fn_solve(listOf([1, 2, 3, 4])), listOf([1, 2, 3, 4]))
}

@Test
func case2() {
    @Expect(typed_fn_solve(listOf([4, 3, 2, 1])), listOf([4, 4, 4, 4]))
}

@Test
func case3() {
    @Expect(typed_fn_solve(listOf([3, 2, 3, 100, 3])), listOf([3, 3, 3, 100, 100]))
}

@Test
func case4() {
    @Expect(typed_fn_solve(listOf([7])), listOf([7]))
}

@Test
func case5() {
    @Expect(typed_fn_solve(listOf([-2, -5, -1])), listOf([-2, -2, -1]))
}

@Test
func case6() {
    @Expect(typed_fn_solve(listOf([1, 1, 1])), listOf([1, 1, 1]))
}

@Test
func case7() {
    @Expect(typed_fn_solve(listOf([0, -1, 2])), listOf([0, 0, 2]))
}
