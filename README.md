## 这是一个开发日志，记录Ataxx的开发

### 准备工作：

这个Ataxx我想用神经网络辅助蒙特卡洛搜索来执行，并且在后续添加高性能计算模块进行辅助训练AI，所以目前需要学习的有：

- 神经网络（打算用卷积神经网络）、蒙特卡洛搜索树

---



```


以下是我找的一种训练方法，用AI解释了一下，我们可以仿照这个方法来训练AI

KataGo 是一个用于围棋的开源人工智能系统，基于深度学习和强化学习，类似于 AlphaGo 但比其更高效、灵活且强大。KataGo 的方法主要结合了 **深度卷积神经网络** 和 **蒙特卡洛树搜索**（MCTS）。它在 **AlphaGo Zero** 的基础上做了改进，并将其方法广泛应用到多个棋类，包括围棋和其他策略游戏。

KataGo 通过以下几个核心技术实现了其强大的性能：

### **1. 深度卷积神经网络（CNN）**

KataGo 使用深度卷积神经网络（CNN）来评估棋盘的局面。网络的主要作用是输出两个东西：

- **局面评估（Value）：** 这个值表示当前局面对于玩家（白方或黑方）的胜利概率。
  
- **行动策略（Policy）：** 这个值表示每个可能走子的概率。
  

#### 网络结构：

KataGo 的网络结构有一些变化，以适应不同的棋盘尺寸和复杂的游戏策略。网络由多个 **卷积层** 和 **残差块（residual blocks）** 组成，类似于 AlphaGo Zero，但结构进行了微调以适应更大棋盘（例如 19x19 围棋）。

#### 关键特性：

- 使用 **卷积层** 来处理局面（棋盘）数据。
  
- 用 **残差连接** 来避免深层网络的梯度消失问题。
  
- 引入 **平衡训练数据**，使得 AI 能够从各种局面中学习，而不仅仅是胜利的局面。
  

### **2. 蒙特卡洛树搜索（MCTS）**

KataGo 结合了蒙特卡洛树搜索（MCTS）来扩展其策略。MCTS 通过多次模拟局面的发展来评估最优的走法。具体来说，它通过以下步骤执行：

1. **选择阶段（Selection）**：在当前搜索树中，根据每个节点的统计信息选择最佳的子节点。
  
2. **扩展阶段（Expansion）**：如果选择的节点是未完全展开的节点，就会进行扩展，即添加新的子节点。
  
3. **模拟阶段（Simulation）**：进行随机模拟（自对弈），直到游戏结束。
  
4. **反向传播阶段（Backpropagation）**：根据模拟结果更新路径上的所有节点的统计信息，传播胜负信息。
  

#### KataGo 中的改进：

- 使用神经网络来引导 MCTS 搜索，而不是单纯依赖于随机模拟，显著提高了搜索效率和精度。
  
- 神经网络为 MCTS 提供了**策略网络**（Policy）和**价值网络**（Value）。策略网络用于指导搜索过程中需要探索哪些走法，而价值网络用于评估局面的好坏。
  

### **3. 强化学习与自我对弈**

KataGo 使用强化学习进行自我对弈训练。通过与自己对弈，KataGo 学习到了不断改进的策略。与 AlphaGo Zero 类似，KataGo 在训练过程中不依赖任何人工标签数据，而是通过自我对弈不断优化自己的策略。

### **4. 对比 AlphaGo Zero**

- **AlphaGo Zero** 采用的是**单一的价值网络**，用于评估局面并做出决策。而 KataGo 在此基础上加入了 **额外的评估**，如**多目标策略**，考虑棋盘的 **对称性** 和 **平衡性**，从而提升了策略的多样性和准确性。
  
- **KataGo** 的训练通过强化学习不断改进，具有比 AlphaGo 更加灵活的训练模式，适应不同棋盘和不同棋类。
  

---

### **5. KataGo 的关键模块**

1. **棋盘表示（Board Representation）**：使用一种高效的棋盘表示来减少内存占用和加速计算。
  
2. **神经网络推理（Network Inference）**：利用 GPU 高效推理，快速评估局面和生成策略。
  
3. **树搜索（Tree Search）**：基于 MCTS 进行多层次搜索，结合神经网络的建议进行优化。
  
4. **并行化**：KataGo 支持多线程和分布式计算，从而提高搜索效率和评估速度，尤其适合 **高性能计算平台**。
  

---

### **6. 实际应用**

KataGo 的应用非常广泛，包括：

- **围棋 AI**：作为围棋AI，KataGo 已经在许多比赛中表现出色，甚至击败了顶级围棋选手。
  
- **其他棋类**：尽管 KataGo 最初是为围棋设计的，但其框架也可以很容易地移植到其他棋类中，例如象棋、国际象棋等。
  

---

### **总结**

KataGo 的方法将深度神经网络与蒙特卡洛树搜索结合，并通过自我对弈和强化学习不断优化其策略。其通过引入神经网络来引导 MCTS、并行计算优化和高效的局面评估，达到了极高的棋类游戏水平。KataGo 不仅在围棋领域表现卓越，而且为未来的AI棋类开发提供了非常重要的技术参考。

---
```





- 除此之外我打算在此基础上辅助高性能计算，目前我在学习CUDA的相关内容，但是考虑到CUDA只能在nvidia显卡上运行，所以这部分我来搞定吧！

**Note:** 在神经网络的训练当中，最主要的就是神经网络的权重，我们需要通过训练、调整奖励函数，来不断更新权重，使得它逼近最优解，最终生成一个二进制文件（network_weights.bin），也需要提交到网站当中，相关的内容在书《深度学习入门》当中有所体现，可以看一下，内容不太多，在这个项目当中还是比较有用的，可以稍微看一下，了解个大概，有需要的时候知道要往哪方面想，再查资料或者问AI，虽然是用python写的，但是好在代码比较易读，我没有python基础的情况下辅助着AI可以弄懂。



---

以下作为记录开发日志，可以先把你的代码提交到仓库当中，然后稍微写写说明，方便我们维护版本

### 蒙特卡洛的优化

3.30 对蒙特卡洛算法进行了优化,主要是优化了评估函数,目前的权重设置依然存在问题,无法找到最优解.
> 目前存在的问题:
> - 蒙特卡洛的搜索优化,现在的搜索深度最深是30,限制步数为100,后续要再进行优化,现在大部分时间只能搜到局部解,并且不是最优.
> - 不是最优解的情况可能有几个原因,但是我认为最重要的还是评估函数的调整,今天之内我会把评估函数调整好,使得它可以达到朴素算法的水平,没那么蠢
> - 现在的搜索深度依然不是极限,上午测试过之前的算法的极限大概就1500,当然随着评估函数的复杂化,这个极限会降低.
