package com.project.backend.controller;

import org.springframework.stereotype.Controller;
import org.springframework.web.bind.annotation.GetMapping;

@Controller
public class FrontendController {
    @GetMapping("/login")
    public String loginPage() {
        return "forward:/index.html";
    }
}
