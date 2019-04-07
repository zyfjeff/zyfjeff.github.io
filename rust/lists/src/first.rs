use std::mem;

pub struct List {
    head: Link,
}

enum Link {
    Empty,
    More(Box<Node>),
}

struct Node {
    elem: i32,
    next: Link,
}

impl List {
  pub fn new() -> Self {
    return List { head: Link::Empty };
  }

  pub fn pop(&mut self) -> Option<i32> {
    // replace的应用，不能先self.head移动给其他，然后再给self.head赋予新的值
    match mem::replace(&mut self.head, Link::Empty) {
      Link::Empty => None,
      Link::More(node) => {
        self.head = node.next;
        Some(node.elem)
      }
    }
  }

  pub fn push(&mut self, elem: i32) {
    let new_node = Box::new(Node {
      elem: elem,
      // 同上，不能先next: self.head，然后再给self.head赋值
      next: mem::replace(&mut self.head, Link::Empty),
    });

    self.head = Link::More(new_node);
  }
}


impl Drop for List {
    fn drop(&mut self) {
        let mut cur_link = mem::replace(&mut self.head, Link::Empty);
        // `while let` == "do this thing until this pattern doesn't match"
        while let Link::More(mut boxed_node) = cur_link {
            cur_link = mem::replace(&mut boxed_node.next, Link::Empty);
            // boxed_node goes out of scope and gets dropped here;
            // but its Node's `next` field has been set to Link::Empty
            // so no unbounded recursion occurs.
        }
    }
}

#[cfg(test)]
mod test {
    use super::List;
    #[test]
    fn basics() {
      let mut list = List::new();

      // Check empty list behaves right
      assert_eq!(list.pop(), None);

      // Populate list
      list.push(1);
      list.push(2);
      list.push(3);

      // Check normal removal
      assert_eq!(list.pop(), Some(3));
      assert_eq!(list.pop(), Some(2));

      // Push some more just to make sure nothing's corrupted
      list.push(4);
      list.push(5);

      // Check normal removal
      assert_eq!(list.pop(), Some(5));
      assert_eq!(list.pop(), Some(4));

      // Check exhaustion
      assert_eq!(list.pop(), Some(1));
      assert_eq!(list.pop(), None);
    }
}